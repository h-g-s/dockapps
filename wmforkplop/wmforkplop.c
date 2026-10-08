/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
  USA.
*/
  
#include "config.h"
#include <string.h>
#include <sys/types.h>
#include <sys/times.h>
#include <pwd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#define GLOBALS_HERE
#include "global.h"
#include "wmforkplop.h"

/* Modified in this fork on 2026-10-08: load the bundled font before fallback. */

uid_t euid, uid;
struct tms tms;
int is_in_rect(int x, int y, int x1, int y1, int w, int h) {
  return (x >= x1 && x < x1+w && y >= y1 && y < y1+h);
}

void update_io_matrix_rw(App *app, int count, int op) {
  IOMatrix *io = &app->iom;
  IO_op_lst *l;
  while (count-- > 0) {
    l = calloc(1,sizeof(*l)); assert(l);
    l->next = io->ops;
    l->n = 10;
    l->op = op;
    l->i = rand() % io->h; 
    l->j = rand() % io->w;
    io->ops = l;
  }
}

void update_io_matrix(App *app) {
  update_io_matrix_rw(app, get_fork_count(), OP_READ);
  update_io_matrix_rw(app, get_kill_count(), OP_WRITE);
}

#define IOSCAL 32768

void evolve_io_matrix(App *app, DATA32 * __restrict buff) {
  IOMatrix *io = &app->iom;
  int i,j;
  int * __restrict pl, * __restrict tmp;
  IO_op_lst *o = io->ops, *po = NULL, *no;
  while (o) {
    io->v[o->i+1][o->j+1] = ((o->op == OP_READ) ? +5000000 : -5000000);
    no = o->next;
    if (--o->n <= 0) {
      if (po) { po->next = no; }
      else { io->ops = no; }
      free(o);
    } else po = o;
    o = no;
  }
  static int cnt = 0;
  
  static float a,b,c,d;
  if (cnt++ % 200 == 0) {
    switch (rand() % 8) {
      //case 1 : a = c = 3; b = d = 1; break;
      //case 2 : a = c = 1; b = d = 3; break;
      case 3 : a = 3; c = b = d = 5./3; break;
      case 4 : b = 3; c = a = d = 5./3; break;
      case 5 : c = 3; a = b = d = 5./3; break;
      case 6 : d = 3; c = b = a = 5./3; break;
      default : a = b = c = d = 2; break;
    }
  }

  /* brain-dead diffusion */
  pl = io->v[io->h+2];
  int * __restrict l = io->v[io->h+3];
  for (j=1; j < io->w+1; ++j) pl[j] = 0;  
  for (i=1; i < io->h+1; ++i) {
    /*
    for (j=1; j < io->w+1; ++j) {
      l[j] = io->v[i][j]*0.99 + (io->v[i][j-1] + io->v[i][j+1] + pl[j] + io->v[i+1][j] - 4*io->v[i][j])/5.;
    }
    */
  /*  
    float *__restrict nl = io->v[i+1]+1;
    float *__restrict cl = io->v[i];
    for (j=1; j < io->w+1; ++j, ++cl) {
      l[j] = (cl[1])*0.99 + (cl[0] + cl[2] + pl[j] + *nl++ - 4*cl[1])/5.;
    }
*/    
    int *__restrict dest;
    int *__restrict pn = io->v[i+1]+1;
    int *__restrict pc = io->v[i]+1;
    int *__restrict pp = pl+1;
    int pj,cj=0,nj=*pc++;
    for (j=0, dest=l+1, pj = 0.; j < io->w; ++j) {
      pj = cj; cj = nj; nj = *pc++;
      *dest = (cj * 37)/200 + (a*pj + c*nj + b*(*pp++) + (*pn++)*d)/10;
      /* *dest = 99*(cj + (pj + nj + *pp++ + *pn++)/(4))/200; */
    //} for (j=0, dest=l+1; j < io->w; ++j) {
      int v = (int)(*dest++ >> 10);
      if (v == 0) { *buff++ = io->cm.p[CMAPSZ/2]; continue; }
      if (v > CMAPSZ/4) { /* cheaper than a log(vv) ... */
        v = MIN(CMAPSZ/4  + (v-CMAPSZ/4)/16,CMAPSZ/2-1);
      } else if (v < -CMAPSZ/4) {
        v = MAX(-CMAPSZ/4 + (v+CMAPSZ/4)/16,-CMAPSZ/2);
      }
      *buff++ = io->cm.p[v+CMAPSZ/2];
    }
      
    tmp = pl; pl = io->v[i]; io->v[i] = l; l = tmp;
    io->v[io->h+2] = pl; io->v[io->h+3] = l;
  }
}


void sethw(App *app, int lw, int lh, int pos, int *px, int *py, int *pw, int *ph) {
  *px = *py = 0; *pw = lw; *ph = lh;
  if (!(pos & (AL_RIGHT | AL_LEFT | AL_HCENTER))) *pw = app->dock->w;
  if (pos & AL_RIGHT) *px = app->dock->w - lw;
  else if (pos & AL_HCENTER) *px = (app->dock->w - lw)/2;
  else if (pos & AL_LEFT) *px = 0;
  else { *px = 0; *pw = app->dock->w; }
  if (pos & AL_BOTTOM) *py = app->dock->h - lh;
  else if (pos & AL_VCENTER) *py = (app->dock->h - lh)/2;
  else if (pos & AL_TOP) *py = 0;
  else { *py = 0; *ph = app->dock->h; }
}

static void my_imlib_text_draw(int x, int y, const char *s) {
  char s2[100]; snprintf(s2,100,"%s ",s); /* my imlib2 often forgets half of the last character... */
  //int i; for (i=0; s2[i]; ++i) s2[i] = toupper(s2[i]);
  imlib_text_draw(x,y,s2);
}


typedef struct {
  pid_t pid;
  int dir, alpha, decnt, dead;
  char cmd[40];
  int mem, time;
  uid_t uid;
  float cpu;
  //pinfo *p; /* very bad idea.. potential race conditions under fork storms */
  int sx, sy, sw, sh;
} slot_info;

static int nb_slots = 0;
static slot_info *slot = NULL;

const char *pretty_print_mem(guint64 iv) {
  static char buff[10];
  double v = iv;
  if (iv < 1024) { snprintf(buff, sizeof buff, "%.1fk", v/1024); }
  else if (iv < 999*1024) { snprintf(buff, sizeof buff, "%.0fk", v/1024); }
  else if (iv < 9*1024*1024) { snprintf(buff, sizeof buff, "%.1fM", v/(1024*1024)); }
  else if (iv < 999*1024*1024) { snprintf(buff, sizeof buff, "%.0fM", v/(1024*1024)); }
  else if (v < 9.*1024*1024*1024) { snprintf(buff, sizeof buff, "%.1fG", v/(1024*1024*1024.)); }
  else { snprintf(buff, sizeof buff, "%3.0fG", v/(1024*1024*1024.)); } /* should not happen often */
  return buff;
}

static void update_slot_info(slot_info *s, pinfo *p) {
  s->dead = 0;
  s->cpu = cpu_usage(p);
  s->mem = p->mem.vsize;
  s->time = (int)((p->time.utime+p->time.stime)/p->time.frequency);
  s->uid = p->state.uid;
  strncpy(s->cmd, p->state.cmd, 40); s->cmd[39] = 0; 
}

static void draw_toplist(App *app) {
  if (Prefs.hdlist_pos == AL_NONE) return;
  if (!app->smallfont) return;
  
  static int lw = -1, lh = -1, lx = -1, ly = -1;
  int use_long_lines, slot_h;
  if (app->dock->w >= 105) {
    slot_h = app->fn_h; use_long_lines = 1;
  } else {
    slot_h = app->fn_h*2; use_long_lines = 0;
  }
  if (app->displayed_hd_changed) { lx = -1; app->displayed_hd_changed = 0; }
  imlib_context_set_font(app->smallfont);

  const GList *l;
  int old_displayed = 0, max_displayed = 0, nb_free_slots = 0, i;

  /* setup slots array */
  max_displayed = app->dock->h/(slot_h+2);
  if (max_displayed != nb_slots || slot == NULL) {
    slot = realloc(slot, sizeof(slot_info)*max_displayed);
    for (i = nb_slots; i < max_displayed; ++i) { 
      slot[i].pid = (pid_t)(-1); slot[i].alpha = slot[i].dir = 0; slot[i].dead = 0; slot[i].cmd[0] = 0;
    }
    nb_slots = max_displayed;
  }
  slot_info *smallest_cpu_slot = NULL;
  /* count "free" slots and set positions */
  for (i = 0, old_displayed = 0; i < nb_slots; ++i) {
    slot[i].sw = lw; slot[i].sh = slot_h+2;
    slot[i].sx = lx; slot[i].sy = ly + lh - slot[i].sh*(nb_slots-i);
    if (slot[i].pid != (pid_t)(-1)) {
      slot[i].alpha += slot[i].dir;
      if (slot[i].alpha <= 0) {
        slot[i].pid = (pid_t)(-1);
      } else {
        if (slot[i].alpha >= 255) slot[i].alpha = 255;
        old_displayed++;
      }
      if (slot[i].decnt > 0) slot[i].decnt--;
      if (slot[i].decnt == 0)
        if (slot[i].dir > 0) slot[i].dir = -25;
    }
    if (slot[i].pid == (pid_t)(-1)) nb_free_slots++;
    else {
      /* update state of displayed slots */
      pinfo *p = proc_hash_find_pid(slot[i].pid);
      if (p) {
        float cpu = cpu_usage(p);
        if (cpu > Prefs.toplist_threshold) { 
          slot[i].dir = +90; 
          slot[i].decnt = Prefs.toplist_remanence;
        }
        update_slot_info(&slot[i], p);
        if (smallest_cpu_slot == NULL || smallest_cpu_slot->cpu > slot[i].cpu)
          smallest_cpu_slot = &slot[i];
      } else { slot[i].dead = 1; }
    }
  }
  old_displayed = MIN(old_displayed, max_displayed);

  //if (smallest_cpu_slot) printf("smallest : %s %f %d\n", smallest_cpu_slot->cmd, smallest_cpu_slot->cpu, nb_slots);
    
  /* fill empty slots with new top cpu consuming processes */
  for (l = get_top_processes(); l; l = l->next) {
    pinfo *p = (pinfo*)l->data;
    float cpu = cpu_usage(p);
    if (cpu < Prefs.toplist_threshold) break; /* no more candidates ? */
    int ok = 0;

    if (p->time.flags == 0) continue;
    for (i = 0; i < nb_slots; ++i)
      if (slot[i].pid == p->pid) break; /* this one is known */ 
  
    if (i != nb_slots) continue;

    /* pid not found in current list, the process is inserted int the first free slot */
    for (i = nb_slots-1; i >= 0; --i) {
      if (slot[i].pid == (pid_t)(-1)) { 
        slot[i].pid = ((pinfo*)l->data)->pid; slot[i].dir = +90; slot[i].alpha = 50; slot[i].decnt = Prefs.toplist_remanence;
        ok = 2;
        break;
      }
    }
    /* no free slot, we search for a candidate */
    if (!ok) {
      for (i = 0; i < nb_slots; ++i) {
        if (slot[i].cpu < Prefs.toplist_threshold/2 && slot[i].decnt < Prefs.toplist_remanence/2) {
          slot[i].pid = ((pinfo*)l->data)->pid; slot[i].dir = +90; slot[i].decnt = Prefs.toplist_remanence;
          ok = 1;
          break;
        }
      }
    }
    /* a relatively low cpu consuming process is blocking the slot ? */
    if (!ok && smallest_cpu_slot && cpu > 2*smallest_cpu_slot->cpu) {
      i = smallest_cpu_slot - slot;
      //printf("arg.. replacing %s %.3f with %s %.3f\n", slot[i].cmd, slot[i].cpu, p->state.cmd, cpu);
      slot[i].pid = ((pinfo*)l->data)->pid; slot[i].dir = +90; slot[i].decnt = Prefs.toplist_remanence;
      ok = 1;
    }
    if (ok) update_slot_info(&slot[i], p);
    if (ok != 2) break;
  }
  
  /* draw the slots */
  lw = app->dock->w; 
  lh = nb_slots * slot_h;
  sethw(app,lw,lh,Prefs.hdlist_pos,&lx,&ly,&lw,&lh);
  
  for (i = 0; i < nb_slots; ++i) {    
    if (slot[i].pid == (pid_t)(-1)) continue;
    imlib_context_set_color(100, 100, 100, (180 * slot[i].alpha)/255);
    imlib_image_fill_rectangle(slot[i].sx,slot[i].sy,lw,slot[i].sh);

    //imlib_context_set_color(100, 100, 100, (200 * slot[i].alpha)/255);
    //imlib_image_draw_rectangle(slot[i].sx,slot[i].sy,lw,slot[i].sh+1);
  
    char s[200];
    int w1,w2,h;
    if (slot[i].uid == 0) {
      imlib_context_set_color(255, 100, 200, slot[i].alpha);
    } else if (slot[i].uid == uid) {
      imlib_context_set_color(255, 255, 255, slot[i].alpha);
    } else imlib_context_set_color(100, 255, 100, slot[i].alpha);
    if (use_long_lines) imlib_context_set_cliprect(slot[i].sx,0,app->dock->w - 68, app->dock->h);
    my_imlib_text_draw(slot[i].sx,slot[i].sy, slot[i].cmd);
    if (use_long_lines) imlib_context_set_cliprect(slot[i].sx,slot[i].sy, 0, 0);
    int t = slot[i].time;
    const char *unit = "s";
    if (t >= 100) { 
      t /= 60; unit = "m"; 
      if (t >= 100) {
        t /= 60; unit = "h";
        if (t >= 100) { t /= 24; unit = "d"; }
      }
    }
    snprintf(s, sizeof s, "%dM", slot[i].mem/1024/1024);
    if (!slot[i].dead) {
      imlib_context_set_color(200, 255, 255, (200*slot[i].alpha)/255);
    } else imlib_context_set_color(180, 180, 180, slot[i].alpha);
    my_imlib_text_draw((use_long_lines == 0 ? (int)slot[i].sx : (int)(app->dock->w - 64)), slot[i].sy+app->fn_h*(1-use_long_lines), s);

    if (slot[i].cpu < 1.) 
      snprintf(s,sizeof s,"%2d%%",(int)(slot[i].cpu*100.));
    else snprintf(s,sizeof s,"%3d",(int)(slot[i].cpu*100.));
    imlib_get_text_size(s,&w2,&h);
    if (!slot[i].dead) {
      imlib_context_set_color(255, 250, 100, slot[i].alpha);
    } else imlib_context_set_color(180, 180, 180, slot[i].alpha);
    my_imlib_text_draw(slot[i].sx + lw - w2 - 2, slot[i].sy+h*(1-use_long_lines), s);

    if (app->dock->w > 48) {
      snprintf(s, sizeof s, "%d%s ", t, unit);
      imlib_get_text_size(s,&w1,&h);    

      if (!slot[i].dead) {
        imlib_context_set_color(100, 250, 100, slot[i].alpha);
      } else imlib_context_set_color(180, 180, 180, slot[i].alpha);
      my_imlib_text_draw(slot[i].sx + lw - w1 - app->fn_w*4 - 1, slot[i].sy+h*(1-use_long_lines), s);
    }

  }
}

const char *fmt_hh_mm_ss(float sec, int deci) {
  static char s[40];
  int min,hour;
  min = (int)(sec/60); sec -= (min * 60);
  hour = min/60; min %=60;
  if (deci) 
    snprintf(s, sizeof s, "%d:%02d:%04.1f", hour, min, sec);
  else snprintf(s, sizeof s, "%d:%02d:%02d", hour, min, (int)sec);
  return s;
}

static void draw_single_proc_info(App *app) {
  pinfo *p = app->single_pid_mode;
  char s[200];
  int ww, hh, advancex, advancey, cy, use_long_lines, xcol2;
  imlib_context_set_color(200, 200, 200, 255);

  use_long_lines = app->dock->w > 105 ? 1 : 0;
  xcol2 = use_long_lines ? app->dock->w/2 : 0;
  cy = 3;

  snprintf(s, sizeof s, "%s%s", (app->dock->w > 56) ? "usr:" : "", getpwuid(p->state.uid)->pw_name);
  my_imlib_text_draw(1, 3+app->fn_h*(use_long_lines ? 1 : 2), s);

  {
    float c=cpu_usage(p);
    int ycpu = 3+app->fn_h*(use_long_lines ? 2 : 3);
    int yrun = 3+app->fn_h*(use_long_lines ? 2 : 4);
    /*double t = (p->locked > 0 ? static glibtop_uptime uptime; 
      if (p->locked >0) glibtop_get_uptime(&uptime);*/

    imlib_context_set_color(155, 200, 200, 255);
    if (app->dock->w > 56) {
      imlib_text_draw_with_return_metrics(1, ycpu, "cpu:", &ww, &hh, &advancex, &advancey);
      imlib_text_draw_with_return_metrics(1+xcol2, yrun, "run:", &ww, &hh, &advancex, &advancey);
    } else { advancey = advancex = 0; }

    //my_imlib_text_draw(1+advancex, 3+app->fn_h*4, fmt_hh_mm_ss(uptime.uptime - p->time.start_time/(float)p->time.frequency, 0));
    my_imlib_text_draw(1+advancex+xcol2, yrun, fmt_hh_mm_ss(get_runtime(p),1));
    imlib_context_set_color(155+c*100, 200-(c>.3?c:0)*200, 200-(c>.3?c:0)*200, 255);
    my_imlib_text_draw(1+advancex, ycpu, fmt_hh_mm_ss(((float)p->time.utime + p->time.stime)/p->time.frequency, 1));
  }
  unsigned currenty = 3+app->fn_h*(use_long_lines ? 3: 5), currentx = 1;
  if (currenty <= app->dock->h - app->fn_h) {
    char svsize[10], sresid[10], smaxmem[10];
    strcpy(svsize, pretty_print_mem(p->mem.vsize));
    strcpy(sresid, pretty_print_mem(p->mem.resident));
    strcpy(smaxmem, pretty_print_mem(vsize_max_achieved(p)));

    imlib_context_set_color(200, 200, 200, 255);
    imlib_text_draw_with_return_metrics(currentx-1, currenty, "sz :", &ww, &hh, &advancex, &advancey);
    currentx += advancex;
    
    int c = MAX(0,vsize_increase(p) / 80000);
    imlib_context_set_color(200 + MIN(c,55), 200-MIN(c,100), 200-MIN(c,100), 255);
    
    imlib_text_draw_with_return_metrics(currentx-1, currenty, sresid, &ww, &hh, &advancex, &advancey);
    currentx += advancex;
    imlib_context_set_color(200, 200, 200, 255);
    imlib_get_text_size(svsize, &ww, &hh);
    my_imlib_text_draw(MIN(app->dock->w,64) - ww - 1, currenty, svsize);
    //imlib_text_draw_with_return_metrics(currentx-1, currenty, sresid, &ww, &hh, &advancex, &advancey);
    //currentx += advancex;
    //snprintf(s, sizeof s, "%s Flt%.0fM/s", pretty_print_mem(vsize_max_achieved(p)), faults_rate(p) * app->page_size / (1024.*1024.));
    //my_imlib_text_draw(1, 3+app->fn_h*6, s);    
    currenty += app->fn_h * (1-use_long_lines);
    snprintf(s, sizeof s, "%.1fkF/s", faults_rate(p)/(1024.));
    my_imlib_text_draw(1+xcol2, currenty, s);
    strcpy(s,pretty_print_mem(vsize_max_achieved(p)));
    imlib_get_text_size(s, &ww, &hh);
    my_imlib_text_draw(app->dock->w - ww - 1, currenty, s);
  }
}

// num = 1..3
int ykill_button(int num) {
  if ((int)app->dock->h > 4*(app->fn_h+2)) {
    return app->dock->h + 1 - (app->fn_h+3)*(4-num);
  } else {
    return 1 + (app->fn_h+3)*num;
  }
}

int is_in_kill_button(int num, int x, int y) {
  //int y1 = app->dock->h + 1 - (app->fn_h+3)*(4-num);
  return is_in_rect(x,y,0,ykill_button(num),app->dock->w,app->fn_h+2);
}

static void draw_kill_mode(App *app) {
  int w, h, y1, y2, y3;
  char s[200];
  
  y1 = ykill_button(1); //app->dock->h + 1 - (app->fn_h+3)*3;
  y2 = ykill_button(2); //app->dock->h + 1 - (app->fn_h+3)*2;
  y3 = ykill_button(3); //app->dock->h + 1 - (app->fn_h+3)*1;

  if (is_in_kill_button(1,app->mx, app->my)) imlib_context_set_color(250, 150, 150, 255);
  else imlib_context_set_color(250, 250, 250, 255);
  imlib_image_fill_rectangle(0, y1 - 1, app->dock->w, app->fn_h+2);
  if (is_in_kill_button(2,app->mx, app->my)) imlib_context_set_color(250, 150, 150, 255);
  else imlib_context_set_color(250, 250, 250, 255);
  imlib_image_fill_rectangle(0, y2 - 1, app->dock->w, app->fn_h+2);
  if (is_in_kill_button(3,app->mx, app->my)) imlib_context_set_color(250, 150, 150, 255);
  else imlib_context_set_color(250, 250, 250, 255);
  imlib_image_fill_rectangle(0, y3 - 1, app->dock->w, app->fn_h+2);

  imlib_context_set_color(0, 0, 0, 255);
  strcpy(s, "KILL GENTLY"); imlib_get_text_size(s, &w, &h);
  my_imlib_text_draw((app->dock->w - w)/2, y1, s);

  strcpy(s, "KILL -9"); imlib_get_text_size(s, &w, &h);
  my_imlib_text_draw((app->dock->w - w)/2, y2, s);

  strcpy(s, "KILLALL"); imlib_get_text_size(s, &w, &h);
  my_imlib_text_draw((app->dock->w - w)/2, y3, s);
}

static void draw_single_pid_mode(App *app) {
  //#define FAULTS_HISTORY 20
  //static guint64 faults[FAULTS_HISTORY] = {0,};
  if (!app->smallfont) return;
  int use_long_lines = app->dock->w > 105 ? 1 : 0;
  int xcol2 = use_long_lines ? app->dock->w/2 : 0;
  int ystate = use_long_lines ? 1 : 3+app->fn_h;

  imlib_context_set_font(app->smallfont);
  pinfo *p = app->single_pid_mode;
  char s[200];
  p->update_stats_decnt = 0;
  if (!app->kill_mode && is_in_rect(app->mx, app->my, 0, 0, app->dock->w, app->fn_h+2)) {
    imlib_context_set_color(250, 250, 250, 220);
    imlib_image_fill_rectangle(0, 0, app->dock->w, app->fn_h+2);
    imlib_context_set_color(0, 0, 0, 255);
    my_imlib_text_draw(3, 1, "KILL ?");
  } else {
    imlib_context_set_color(50, 50, 50, 220);
    imlib_image_fill_rectangle(0, 0, app->dock->w, app->fn_h+2);
    imlib_context_set_color(255, 255, 255, 255);
    my_imlib_text_draw(3, 1, p->state.cmd);
  }
  imlib_context_set_color(50, 50, 50, 130);
  imlib_image_fill_rectangle(0, app->fn_h+3, app->dock->w, app->dock->h);

  const char *sstate = "?";
  char cstate[2];
  if (p->locked == -1) { sstate = "DEAD"; }
  else if (app->dock->w > 56) {
    unsigned s = p->state.state;
    if (s & GLIBTOP_PROCESS_RUNNING)
      sstate = "Run";
    else if (s & GLIBTOP_PROCESS_INTERRUPTIBLE)
      sstate = "Slp";
    else if (s & GLIBTOP_PROCESS_UNINTERRUPTIBLE)
      sstate = "USlp";
    else if (s & GLIBTOP_PROCESS_SWAPPING)
      sstate = "Swap";
    else if (s & GLIBTOP_PROCESS_ZOMBIE)
      sstate = "Zomb";
    else if (s & GLIBTOP_PROCESS_STOPPED)
      sstate = "Stop";
    else if (s & GLIBTOP_PROCESS_DEAD)
      sstate = "Dead";
    else sstate = "?";
#if 0    
    switch (p->state.state) {
      /*
        D uninterruptible sleep (usually IO)
        R runnable (on run queue)
        S sleeping
        T traced or stopped
        W paging (2.4 kernels and older only)
        X dead
        Z a defunct ("zombie") process
      */
      case 'Z': sstate = "Zomb"; break;
      case 'S': sstate = "Slp"; break;
      case 'D': sstate = "USlp"; break;
      case 'T': sstate = "Stop"; break;
      case 'W': sstate = "Swap"; break; 
      case 'R': sstate = "Run"; break; 
      default: {
        if (p->state.state > ' ') { 
          cstate[0] = p->state.state; cstate[1] = 0; sstate = cstate;
        } break;
      }
    }
#endif
  } else { cstate[0] = p->state.state; cstate[1] = 0; sstate = cstate; }
  snprintf(s, sizeof s, "%d", p->pid);
  imlib_context_set_color(250, 250, 250, 220);
  my_imlib_text_draw(1+xcol2, ystate, s);
  if (p->locked >= 0) imlib_context_set_color(255, 250, 100, 255);
  else { 
    int c = ABS(100-((app->tics_now*2) % 200)); imlib_context_set_color(255, c/2, c/2, 155 + c); 
  }
  if (app->dock->w > 48) {
    if (p->locked != -1) 
      snprintf(s, sizeof s, "[%s,%d]", sstate, g_list_length(p->childs));
    else snprintf(s, sizeof s, "[%s]", sstate);
  } else {
    if (p->locked != -1) 
      snprintf(s, sizeof s, "%s%d", sstate, g_list_length(p->childs));
    else snprintf(s, sizeof s, "%s", sstate);
  }
  {
    int w,h;
    imlib_get_text_size(s, &w, &h);
    my_imlib_text_draw(app->dock->w - w - 2, ystate, s);
  }
  if (app->kill_mode) {
    draw_kill_mode(app);
  } else {
    draw_single_proc_info(app);
  }
}

static void draw_throughput(App *app UNUSED) {
#if 0
  static int tpstep = 0, tpw, tph;
  static char tpmsg[20];
  static int lw = -1, lh = -1, lx = -1, ly = -1;
  static int reshape_cnt = 0;
  if (Prefs.popup_throughput_pos == AL_NONE) return;
  
  if (!app->bigfont) return;
  imlib_context_set_font(app->bigfont);
  /* get dimensions (only once) */
  if (lx == -1 || app->reshape_cnt != reshape_cnt) {    
    imlib_get_text_size("00.0M/s",&lw,&lh);
    if (lw > (int)(app->dock->w*3/4)) { lw = app->dock->w; }
    sethw(app,lw,lh,Prefs.popup_throughput_pos,&lx,&ly,&lw,&lh);
    reshape_cnt = app->reshape_cnt;
  }
  
  if (get_read_mean_throughput() + get_write_mean_throughput() > Prefs.popup_throughput_threshold) {
    tpstep = MIN(tpstep+1,4);
    snprintf(tpmsg,sizeof tpmsg, "%.1fM/s",get_read_mean_throughput() + get_write_mean_throughput());
    imlib_get_text_size(tpmsg,&tpw,&tph);
    if (tpw > lw) { 
      snprintf(tpmsg,sizeof tpmsg, "%.1fM",get_read_mean_throughput() + get_write_mean_throughput());
      imlib_get_text_size(tpmsg,&tpw,&tph);
    }
  } else if (tpstep) tpstep--;
  if (tpstep) {
    imlib_context_set_color(128, 128, 128, tpstep*30);
    imlib_image_draw_rectangle(lx-1,ly-1,lw+2,lh+2);
    imlib_context_set_color(128, 128, 128, 10+tpstep*25);
    imlib_image_fill_rectangle(lx,ly,lw,lh);
    imlib_context_set_color(255, 255, 255, 50+tpstep*50);
    my_imlib_text_draw(lx + (lw - tpw)/2, ly, tpmsg);
  }
#endif
}

static void draw(App *app) {
  DATA32 *buff = imlib_image_get_data();

  if (!Prefs.disable_io_matrix) {
    evolve_io_matrix(app,buff);
  } else memset(buff, 0, sizeof(DATA32)*app->dock->w*app->dock->h);
  imlib_image_put_back_data(buff);
  if (app->single_pid_mode == NULL) {
    draw_toplist(app);
  } else {
    draw_single_pid_mode(app);
  }
  draw_throughput(app);
}

void reshape(int w, int h) {
  DockImlib2 *dock = app->dock;
  static int isinit = 0;
  dock->w = w; dock->h = h;
  dock->win_width = dock->w + dock->x0;
  dock->win_height = dock->h + dock->y0;
  app->reshape_cnt++;
  app->iom.w = dock->w; app->iom.h = dock->h;
  if (isinit) FREE_ARR((void*)app->iom.v);
  ALLOC_ARR(app->iom.v,app->iom.h+4, app->iom.w+2);
  if (isinit) { dockimlib2_reset_imlib(dock); }

  isinit = 1;
}

void switch_to_single_pid_mode(pid_t pid) {
  pinfo *p = proc_hash_find_pid(pid);
  GList *l = get_top_processes();
  app->single_pid_mode = p;
  if (p) BLAHBLAH(1, printf("app->single_pid_mode = %ld/%s\n", (long)pid, p->state.cmd));
  
  for ( ; l; l = l->next) {
    pinfo *p_ = (pinfo*)(l->data);
    if (app->single_pid_mode) {
      if (p_->locked == 0) p_->locked = 1;
    } else p_->locked = 0;
  }
}


void do_scroll_up() {
  GList *l = get_alpha_processes();
  if (l) {
    for ( ; l; l = l->next) if (l->next && (pinfo*)(l->next->data) == app->single_pid_mode) break;
    if (!l) app->single_pid_mode = (pinfo*)(g_list_last(get_alpha_processes())->data);
    else app->single_pid_mode = (pinfo*)(l->data);
  }
  app->single_pid_mode->locked = 1;
}

void do_scroll_down() {
  GList *l = get_alpha_processes();
  if (l) {
    for ( ; l; l = l->next) if (l->prev && (pinfo*)(l->prev->data) == app->single_pid_mode) break;
    if (!l) app->single_pid_mode = (pinfo*)(get_alpha_processes()->data);
    else app->single_pid_mode = (pinfo*)(l->data);
  }
  app->single_pid_mode->locked = 1;
}

#ifndef GKRELLM
static void event_loop(App *app) {
  int tic_cnt = 0;
  int dragging = 0, last_click_x = -100, last_click_y = -100;
  while (1) {
    XEvent ev;
    tic_cnt++;
    if (tic_cnt % 5 == 0) {
      XWindowAttributes attr;
      if (app->dock->normalwin) {
        XGetWindowAttributes(app->dock->display, app->dock->normalwin, &attr);
        app->dock->normalwin_mapped = (attr.map_state == IsViewable);
      }
      if (app->dock->iconwin) {
        XGetWindowAttributes(app->dock->display, app->dock->iconwin, &attr);
        app->dock->iconwin_mapped = (attr.map_state == IsViewable);
      }
    }
    app->tics_now = times(&tms); app->time_now = time(NULL);
    while (XPending(app->dock->display)) {
      XNextEvent(app->dock->display, &ev);
      switch (ev.type) {
        case ClientMessage:
          if (ev.xclient.message_type == app->dock->atom_WM_PROTOCOLS
              && ev.xclient.format == 32 
              && (Atom)ev.xclient.data.l[0] == app->dock->atom_WM_DELETE_WINDOW) {
            exit(0);
          } 
          break;
        case MapNotify:
          break;
        case UnmapNotify:
          break;
        case EnterNotify:
          dragging = 0;
          break;
        case LeaveNotify:
          app->mx = app->my = -100;
          break;
        case MotionNotify:
          if (app->single_pid_mode && (ev.xmotion.state & Button1Mask)) {
            if (dragging == 0 && (ABS(last_click_x - ev.xmotion.x)+ABS(last_click_y-ev.xmotion.y))>5) dragging = 1;
            int d = ev.xmotion.y - app->my;
            if (dragging && d < 0) {
              do_scroll_up();
            } else if (dragging && d > 0) {
              do_scroll_down();
            }
          }
          app->mx = ev.xmotion.x; app->my = ev.xmotion.y;
          break;
        case ButtonPress:
          dragging = 0;
          app->mx = ev.xbutton.x; app->my = ev.xbutton.y;
          break;
        case ButtonRelease:
          if (dragging) break;
          dragging = 0;
          last_click_x = ev.xbutton.x; last_click_y = ev.xbutton.y;
          app->mx = ev.xbutton.x; app->my = ev.xbutton.y;
          if (ev.xbutton.button == Button1) { 
            if (app->single_pid_mode == NULL) {
              int i, free_slots = 0;
              for (i=0; i < nb_slots; ++i) { 
                if (slot[i].pid == (pid_t)(-1)) free_slots++;
                if (is_in_rect(ev.xbutton.x,ev.xbutton.y, slot[i].sx, slot[i].sy, slot[i].sw, slot[i].sh)
                    && slot[i].pid != (pid_t)(-1)) {
                  BLAHBLAH(1,printf("Going single mode, pid = %d %s\n", slot[i].pid, slot[i].cmd));
                  switch_to_single_pid_mode(slot[i].pid);
                }
              }
              if (!app->single_pid_mode && get_top_processes() != NULL) {
                switch_to_single_pid_mode(((pinfo*)get_top_processes()->data)->pid);
              }
            } else {
              if (ev.xbutton.y <= app->fn_h) {
                app->kill_mode = 1 - app->kill_mode;
              } else {
                if (app->kill_mode == 0) {
                  int i;
                  switch_to_single_pid_mode((pid_t)(-1));
                  for (i=0; i < nb_slots; ++i) { slot[i].pid = (pid_t)(-1); }
                } else {
                  if (is_in_kill_button(1,ev.xbutton.x,ev.xbutton.y)) {
                    BLAHBLAH(1,printf("KILL-15 [%d] %s\n", app->single_pid_mode->pid, app->single_pid_mode->state.cmd));
                    kill(app->single_pid_mode->pid, SIGTERM);
                  } else if (is_in_kill_button(2,ev.xbutton.x, ev.xbutton.y)) {
                    BLAHBLAH(1,printf("KILL-9 [%d] %s\n", app->single_pid_mode->pid, app->single_pid_mode->state.cmd));
                    kill(app->single_pid_mode->pid, SIGKILL);
                  } else if (is_in_kill_button(3,ev.xbutton.x, ev.xbutton.y)) {
                    do_kill_all(app->single_pid_mode->state.cmd);
                  } else app->kill_mode = 0;
                }
              }
            }
          } else if (ev.xbutton.button == Button4 && app->single_pid_mode) {
            do_scroll_up();
          } else if (ev.xbutton.button == Button5 && app->single_pid_mode) {
            do_scroll_down();
          }
          //if (ev.xbutton.button == Button4) prev_displayed_hd(-1);
          //else if (ev.xbutton.button == Button5 || ev.xbutton.button == Button1) next_displayed_hd(+1);
          break;
        case ConfigureNotify: {
          if (app->dock->iconwin == None && 
              (ev.xconfigure.width != (int)app->dock->win_width || 
               ev.xconfigure.height != (int)app->dock->win_height)) {
            app->dock->w = app->dock->win_width = ev.xconfigure.width;
            app->dock->h = app->dock->win_height = ev.xconfigure.height;
            reshape(ev.xconfigure.width, ev.xconfigure.height); //app->dock->w, app->dock->h, None);
            /*printf("ConfigureNotify : %dx%d %dx%d\n", 
              ev.xconfigure.width, ev.xconfigure.height, 
              app->sm.nrow, app->sm.ncol);*/
          }
        } break;
      }
    } 
    if (tic_cnt % Prefs.update_stats_mult == 0) {
      update_stats();
      if (!Prefs.disable_io_matrix)   update_io_matrix(app);
    }
    //if (tic_cnt > 500) exit(1);
    usleep(TIMER_DELAY_MS * 1000);
    if (app->dock->normalwin_mapped || app->dock->iconwin_mapped) {
      draw(app);
      dockimlib2_render(app->dock); /* cpu < 1/5000 */
    }
  }
}
#endif /* ndef GKRELLM */

void setup_cmap(cmap *m) {
  struct {
    float x0; 
    DATA32 c;
  } colors0[] = {{-128, 0xff8080},
                 {- 70, 0xF00000},
                 { -60, 0xDf0080},
                 { -20, 0x800000},
                 {   0, 0x000000},
                 {  10, 0x008000},
                 {  60, 0xf09000},
                 {  90, 0xffa000},
                 { 116, 0xffd000},
                 { 127, 0xffff00}},
    colors1[] = {{-128, 0xff0000},
                 {- 64, 0x808080},
                 { 0, 0x404040},
                 //{  , 0x000000},
                 //{  0 , 0x000000},
                 {  1, 0x208020},
                 {  64, 0x509050},
                 {+ 90, 0x60C060},
                 {+127, 0x008000}},
      colors2[] = {{-128, 0x400000},
                   { -60, 0xA00000},
                   { -34, 0xff0000},
                   { -16, 0x400000},
                   {   0, 0x000000},
                   {  16, 0x000040},
                   {  34, 0x0000ff},
                   {  60, 0x0000A0},
                   {+128, 0x000040}}, 
      colors3[] = {{-128, 0x500060},
                   { -60, 0x500050},
                   { -34, 0x000000},
                   {   0, 0x000000},
                   {  34, 0x000000},
                   {  60, 0x206020},
                   {+128, 0x205020}},
      colors4[] = {{-128, 0x5000F0},
                   { -70, 0x0000C0},
                   { -50, 0x0000A0},
                   { -40, 0x707090},
                   { -30, 0x000080},
                   { -20, 0x505070},
                   { -10, 0x000060},
                   {   0, 0x000000},
                   {  10, 0x006000},
                   {  20, 0x707070},
                   {  30, 0x008000},
                   {  40, 0x909090},
                   {  50, 0x00A000},
                   {  70, 0x00C000},
                   {+128, 0x20D020}},
        colors5[] = {{-128, 0x0080FF},
                     {- 70, 0x0080F0},
                     { -60, 0x4000DF},
                     { -20, 0x600080},
                     {   0, 0x000000},
                     {  10, 0x008000},
                     {  60, 0xf09000},
                     {  90, 0xffa000},
                     { 116, 0xffd000},
                     { 127, 0xffff00}},
          *cdef = NULL;
#define NB_COLORMAP 5
#define SELMAP(n) \
if (Prefs.iomatrix_colormap == n) { sz = sizeof(colors##n)/sizeof(*colors0); cdef = colors##n; }

  unsigned i, sz=0;
  SELMAP(0); SELMAP(1); SELMAP(2); SELMAP(3); SELMAP(4); SELMAP(5);
  float x0 = cdef[0].x0, x1 = cdef[sz-1].x0;
  for (i = 0; i < sz - 1; ++i) {
    int i0 = (int)((cdef[i].x0-x0)*CMAPSZ/(x1-x0));
    int i1 = (int)((cdef[i+1].x0-x0)*CMAPSZ/(x1-x0));
    int r1 = cdef[i].c>>16 & 0xff, r2 = cdef[i+1].c>>16 & 0xff;
    int g1 = cdef[i].c>> 8 & 0xff, g2 = cdef[i+1].c>> 8 & 0xff;
    int b1 = cdef[i].c     & 0xff, b2 = cdef[i+1].c     & 0xff;
    int j;
    for (j=i0; j <= MIN(i1,CMAPSZ-1); ++j) {
      float alpha = (j-i0+.5)/(float)(i1-i0);
      m->p[j] = 
        (MIN((int)(r1*(1-alpha) + alpha*r2),255)<<16) +
        (MIN((int)(g1*(1-alpha) + alpha*g2),255)<<8) +
        (MIN((int)(b1*(1-alpha) + alpha*b2),255));
      //printf("cmap[%d] = 0x%06x\n", j, m->p[j]);
    }
  }
}

unsigned getpos(const char *pp) {
  char p[2];
  unsigned v = AL_NONE, i;  
  if (!pp || !pp[0]) return AL_NONE;
  if (strlen(pp) > 2) { fprintf(stderr, "invalid position specification: '%s'\n", pp); exit(1); }
  strncpy(p,pp,2);
  if (p[0] == 'c') { char tmp = p[0]; p[0] = p[1]; p[1] = tmp; }
  for (i=0; i < 2 && p[i]; ++i) {
    if (p[i] == 'r') { v |= AL_RIGHT; }
    else if (p[i] == 'l') { v |= AL_LEFT; }
    else if (p[i] == 't') { v |= AL_TOP; }
    else if (p[i] == 'b') { v |= AL_BOTTOM; }
    else if (p[i] == 'c') {
      if (v & (AL_LEFT | AL_RIGHT | AL_HCENTER)) v |= AL_VCENTER; else v |= AL_HCENTER; 
    } else {
      fprintf(stderr, "unknown position specifier: '%c'\n", p[i]); exit(1);
    }
  }
  return v;
}

void init_prefs(int argc, char **argv) {
#ifndef GKRELLM
  /* Prefs already read from the gkrellm config file: */
  Prefs.disable_io_matrix = 0;
  Prefs.iomatrix_colormap = 5
;
  Prefs.hdlist_pos = AL_BOTTOM + AL_LEFT;
  Prefs.smallfontname = NULL;
#endif
  Prefs.toplist_threshold = 0.03;
  Prefs.update_stats_mult = 3;
  Prefs.toplist_remanence = 20;
  Prefs.xprefs.dockapp_size = 64;
  Prefs.verbosity = 0;
  Prefs.xprefs.argc = argc; Prefs.xprefs.argv = argv;
  Prefs.xprefs.flags = 0;
}

#ifndef GKRELLM
void parse_options(int argc, char **argv) {
  enum { OPT_DISPLAY=1, OPT_FONT, OPT_FONTPATH, OPT_32, OPT_56, OPT_48, OPT_NOTOP, OPT_NOFORK, OPT_THRESHOLD};
  static struct option long_options[] = {
    {"help",0,0,'h'},
    {"verbose",0,0,'v'},
    {"version",0,0,'V'},
    {"display",1,0,OPT_DISPLAY},
    {"geometry",2,0,'g'},
    {"font",1,0,OPT_FONT},
    {"fontpath",1,0,OPT_FONTPATH},
    {"colormap",1,0,'c'},
    {"32",0,0,OPT_32},
    {"48",0,0,OPT_48},
    {"56",0,0,OPT_56},
    {"no-top",0,0,OPT_NOTOP},
    {"no-fork",0,0,OPT_NOFORK},
    {"threshold",1,0,OPT_THRESHOLD},
    {"proc-update-interval",1,0,'u'},
    {NULL,0,0,0}
  };
  int long_options_index;
  const char *help = 
    "wmforkplop " VERSION " - monitor processes and forking activity\n"
    "Usage: wmforkplop [options]\n"
    "Option list:\n"
    "  -h, --help      print this.\n"
    "  -v, --verbose   increase verbosity\n"
    "  -V, --version   print version\n"
    "  --fontpath path add a new directory to the font search directory list\n"
    "                  default: --fontpath=/usr/share/fonts/truetype (and subdirectories)\n"
    "                           --fontpath=/usr/share/fonts/ttf      (and subdirectories)\n"
    "                           --fontpath=$HOME/.fonts              (and subdirectories)\n"
    "  --font          fontname/size\n"
    "                  Set the 'small font' name/size in pixel\n"
    "                  (default: --smallfont=Vera/6\n"
    "                  The font name are case-sensitive, and must correspound to the name\n"
    "                  of a .ttf file which can be found in one of the fontpaths\n"
    "                  By default, wmforkplop tries to load the following fonts:\n"
    "                    * Vera/6, Andale_Mono/6, Verdana/6, Trebuchet_MS/7\n"
    "                  However, I *strongly* suggest that you use Vera/6.\n"
    "  -c n, --colormap=n\n"
    "                  select colormap number n (0 <= n <= 5)\n" 
    "  -g[=WxH+x+y], --geometry[=WxH+x+y]\n"
    "                  start in window (i.e. undocked) mode with specified geometry (i.e -g 96x32 or -g 64x64+0+0)\n"
    "  --32,  --48,  --56\n"
    "                  start in a reduced dockapp, for people whose dock is too small too contain 64x64 dockapps\n"
    "  --no-top\n"
    "                  disable the wmtop feature, you will only see the fork animation.\n"
    "  --no-fork\n" 
    "                  disable the fork animation, you will only see the list of top processes.\n"
    "  --threshold=n\n"
    "                  minimum CPU consumption (%) of a process listed in the top-list (default 3%)\n"
    "  -u n, --proc-update-delay=n\n"
    "                  set the delay between two reads of /proc, the default is 150 (milliseconds).\n"
    "                  Setting a small value gives accurate results, but consumes more CPU as reading /proc is quite expensive.\n"
    "See " PKGDATADIR "/README for more details.\n";
  init_prefs(argc, argv);
  while (1) {
    int c;
    c = getopt_long(argc, argv, "g::hvVc:u:",long_options,&long_options_index); /* -g option is handled in dockapp_imlib2 */
    if (c == -1)
      break;
    switch (c) {
      case ':':
      case '?': 
        exit(1);
      case OPT_DISPLAY:
        Prefs.xprefs.flags |= DOCKPREF_DISPLAY; Prefs.xprefs.display = strdup(optarg);
        break;
      case 'g':
        Prefs.xprefs.flags |= DOCKPREF_GEOMETRY; if (optarg) Prefs.xprefs.geometry = strdup(optarg);
        break;
      case 'h':
        puts(help); exit(0);
      case 'v':
        Prefs.verbosity++;
        break;
      case 'V':
        printf("wmforkplop %s\n",VERSION); exit(0);
        break;
      case OPT_FONTPATH:
        printf("add font path: %s\n", optarg);
        imlib_add_path_to_font_path(optarg);
        break;
      case OPT_FONT:
        Prefs.smallfontname = strdup(optarg);
        break;
      case 'c':
        Prefs.iomatrix_colormap = atoi(optarg);
        if (Prefs.iomatrix_colormap > NB_COLORMAP) { fprintf(stderr,"invalid colormap number\n"); exit(1); }
        break;
      case OPT_56:
        Prefs.xprefs.dockapp_size = 56;
        break;
      case OPT_48:
        Prefs.xprefs.dockapp_size = 48;
        break;
      case OPT_32:
        Prefs.xprefs.dockapp_size = 32;
        break;
      case OPT_NOTOP:
        Prefs.disable_top_list = 1;
        break;
      case OPT_NOFORK:
        Prefs.disable_io_matrix = 1;
        break;
      case OPT_THRESHOLD:
        Prefs.toplist_threshold = atof(optarg)/100.;
        break;
      case 'u':
        Prefs.update_stats_mult = MAX(1,atoi(optarg)/TIMER_DELAY_MS);
        break;
      default:
        assert(0);
    }
  }
  if (optind != argc) {
    fprintf(stderr, "unknown option: %s\n", argv[optind]); exit(1);
  }
}
#endif

char *xstrdup(const char *s) {
  if (s) return strdup(s);
  else return NULL;
}

void init_fonts(App *app) {
  char *smallfontlist[] = {"Vera/6","FreeSans/7","Trebuchet_MS/7","Verdana/7","Arial/7", "luxisr/7", NULL};
  if (app->smallfont) {
    imlib_context_set_font(app->smallfont); imlib_free_font(); app->smallfont = NULL;
  }
  app->smallfont = load_font(Prefs.smallfontname, smallfontlist);
  app->fn_h = 0;
  if (app->smallfont) {
    app->current_smallfont_name = strdup(dockimlib2_last_loaded_font());
    imlib_context_set_font(app->smallfont);
    imlib_get_text_size("000000", &app->fn_w, &app->fn_h); app->fn_w /= 6;
  }
}

#ifndef GKRELLM
int main(int argc, char**argv)
#else
int forkplop_main(int width, int height, GdkDrawable *gkdrawable)
#endif
{
  //increase_pid();
  euid = geteuid(); uid = getuid(); seteuid(uid);
  ALLOC_OBJ(app);
  srand(time(NULL));
  /* Initialize options */
#ifndef GKRELLM
  parse_options(argc,argv);
#else
  init_prefs(0, NULL);
#endif
  /* Initialize imlib2 */
#ifndef GKRELLM
  app->dock = dockimlib2_setup(2, 2, Prefs.xprefs.dockapp_size-5, Prefs.xprefs.dockapp_size-5, &Prefs.xprefs);
#else
  app->dock = dockimlib2_gkrellm_setup(0, 0, width, height, &Prefs.xprefs, gkdrawable);
#endif
  app->smallfont = NULL; 
  app->current_smallfont_name = NULL;
  app->reshape_cnt = 0;
  app->single_pid_mode = NULL;
  app->displayed_hd_changed = 1;

  imlib_add_path_to_font_path(PKGDATADIR);
  imlib_add_path_to_font_path(".");
  init_fonts(app);
  
  //app->update_display_delay_ms = 50;
  app->tics_per_sec = sysconf(_SC_CLK_TCK);
  app->tics_now = times(&tms);
  app->page_size = sysconf(_SC_PAGESIZE);
  init_stats(); //app->update_display_delay_ms*1e-3*app->update_stats_mult);
  
  reshape(app->dock->w,app->dock->h);
  app->iom.ops = NULL;
  setup_cmap(&app->iom.cm);
#ifndef GKRELLM
  event_loop(app);
#endif
  return 0;
}

#ifdef GKRELLM
void gkrellm_forkplop_update(int update_options) {
  app->tics_now = times(&tms);
  if (update_options) {
    setup_cmap(&app->iom.cm);
  }
  // update the data:
  update_stats();
  if (!Prefs.disable_io_matrix)
    update_io_matrix(app);
  // draw the Imlib2 pixmap:
  draw(app);
  dockimlib2_render(app->dock);
}
#endif
