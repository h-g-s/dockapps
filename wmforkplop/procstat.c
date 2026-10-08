#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include "global.h"
#include "procstat.h"
#include "wmforkplop.h"

/* Modified in this fork on 2026-10-08: use a valid descending sort order. */

static void logged_data_init(logged_data *d, int nb_slices) {
  d->nb_slices = nb_slices;
  d->tic = g_new0(guint64, nb_slices);
  d->data = g_new0(guint64, nb_slices);
  d->max_val = 0;
  d->slice_cnt = 0;
}

static void logged_data_free(logged_data *d) {
  if (d->tic) g_free(d->tic); d->tic = NULL;
  if (d->data) g_free(d->data); d->data = NULL;
}

static void
logged_data_push(logged_data *d, guint64 v) {
  if (v > d->max_val) d->max_val = v;
  if (d->slice_cnt != 0) {
    if ((guint64)app->tics_now == d->tic[(d->slice_cnt-1) % d->nb_slices]) {
      d->data[(d->slice_cnt-1) % d->nb_slices] = v;
      return;
    }
  }
  d->data[d->slice_cnt % d->nb_slices] = v;
  d->tic[d->slice_cnt % d->nb_slices] = app->tics_now;
  d->slice_cnt++;
}

static double
logged_data_mean(logged_data *d) {
  if (d->slice_cnt == 0) return 0.;
  int i0 = MAX(d->slice_cnt - d->nb_slices,0) % d->nb_slices;
  int i1 = (d->slice_cnt-1) % d->nb_slices;
  if (d->tic[i1] == d->tic[i0]) return 0.;
  return ((double)d->data[i1] - (double)d->data[i0])/(d->tic[i1]-d->tic[i0]);
}

 guint64
logged_data_get_current(logged_data *d) {
  if (d->slice_cnt == 0) return 0;
  else return d->data[(d->slice_cnt-1) % d->nb_slices];
}

static pinfo *pinfo_new(int pid) {
  pinfo *p = g_new0(pinfo,1);
  logged_data_init(&p->lcpu, 5); 
  logged_data_init(&p->lsize, 20); 
  logged_data_init(&p->lfaults, 20);
  p->childs = NULL;
  p->parent_aware = +1; /* request for addition into parent child list */
  p->pid = pid;
  return p;
}

static void pinfo_destroy_(pinfo *p) {
  logged_data_free(&p->lcpu);
  logged_data_free(&p->lsize);
  logged_data_free(&p->lfaults);
  g_list_free(p->childs);
  memset(p, sizeof(pinfo), 0);
  g_free(p);
}

#define pinfo_destroy(p) { pinfo_destroy_(p); p = NULL; }

static GHashTable *proc_hash;
static int nbpid_old, nbpid_new;
static int last_pid_old, last_pid_new;
static int fork_count = 0, kill_count = 0;

int get_fork_count() {
  int n = fork_count; fork_count = 0;
  return n;
}
int get_kill_count() {
  int n = kill_count; kill_count = 0;
  return n;
}



pinfo *
proc_hash_find_pid(int pid) {
  if (proc_hash == NULL) return NULL;
  return (pinfo*) g_hash_table_lookup(proc_hash, &pid);
}

static pinfo *
proc_hash_new_pid(int pid, int **ppkey) {
  int *key = g_malloc(sizeof(int)); *key = pid;
  g_assert(proc_hash_find_pid(pid) == NULL);
  pinfo *p = pinfo_new(pid);
  g_hash_table_insert(proc_hash, key, p);
  if (ppkey) *ppkey = key;
  return p;  
}

double get_uptime() {
  static double uptime_base;
  static clock_t tup = 0;
  if (tup == 0 || app->tics_now - tup > 2000) {
    glibtop_uptime uptime; 
    glibtop_get_uptime(&uptime);
    uptime_base = uptime.uptime;
    tup = app->tics_now;

  }
  return uptime_base + (app->tics_now - tup)/(double)app->tics_per_sec;
}

double get_runtime(pinfo *p) {
  double tdeath = (p->locked >= 0) ? 0. : ((long)(app->tics_now - p->death_tics))/(double)app->tics_per_sec;
  //                 if (p->pid == 2889 || 1) { printf("uptime: %g , now=%12f start=%12f, p=%s\n", get_uptime(), (double)app->time_now, (double)p->time.start_time, p->state.cmd); }
  //return get_uptime() - tdeath - p->time.start_time/(double)p->time.frequency;  
  return (app->time_now - p->time.start_time) - tdeath;
}

void update_fork_stats() {
  glibtop_loadavg l;
  nbpid_old = nbpid_new; last_pid_old = last_pid_new;
  glibtop_get_loadavg (&l);
  nbpid_new = l.nr_tasks; last_pid_new = l.last_pid;
  fork_count = MAX(last_pid_new - last_pid_old, 0);
  kill_count = MAX((nbpid_old - nbpid_new) + fork_count,0);
  if (kill_count < 0) g_print("WARNING : kill_cound = %d\n", kill_count);
  //printf("last_pid = %d FORK=%+d, KILL=%+d\n", last_pid_new, fork_count, kill_count);
}

float cpu_usage(pinfo *p) {
  return logged_data_mean(&p->lcpu) * app->tics_per_sec / (float)p->time.frequency;
}

gint64 vsize_increase(pinfo *p) {
  return logged_data_mean(&p->lsize) * app->tics_per_sec;
}

guint64 vsize_max_achieved(pinfo *p) {
  return p->lsize.max_val;
}

float faults_rate(pinfo *p) {
  return logged_data_mean(&p->lfaults) * app->tics_per_sec;
}

/* return a number > 0 if the first parameter comes after the second parameter
   in the sort order. */
static gint compare_cpu_(pinfo *p1, pinfo *p2) {
  float cpu1 = cpu_usage(p1);
  float cpu2 = cpu_usage(p2);
  if (cpu1 > cpu2) return -1;
  if (cpu1 < cpu2) return 1;
  return (p1->pid > p2->pid) - (p1->pid < p2->pid);
}
static gint compare_cmd_(pinfo *p1, pinfo *p2) {
  return strcasecmp(p1->state.cmd, p2->state.cmd);
}
static void get_top_processes_(unsigned *ppid UNUSED, pinfo *p, GList **pl) {
  (*pl) = g_list_insert_sorted(*pl, p, (GCompareFunc)compare_cpu_);
}

static void get_alpha_processes_(unsigned *ppid UNUSED, pinfo *p, GList **pl) {
  (*pl) = g_list_insert_sorted(*pl, p, (GCompareFunc)compare_cmd_);
}

int top_list_is_valid = 0;
int alpha_list_is_valid = 0;

/* cpu usage sorted list */
GList *get_top_processes() {
  static GList *l = NULL;
  if ((top_list_is_valid || app->single_pid_mode) && l) return l;
  if (l) { g_list_free(l); l = NULL; }
  g_hash_table_foreach(proc_hash, (GHFunc)get_top_processes_, &l);
  top_list_is_valid = 1;
  return l;
}

/* alphabetically sorted list */
GList *get_alpha_processes() {
  static GList *l = NULL;
  if ((alpha_list_is_valid || app->single_pid_mode) && l) return l;
  if (l) { g_list_free(l); l = NULL; }
  g_hash_table_foreach(proc_hash, (GHFunc)get_alpha_processes_, &l);
  alpha_list_is_valid = 1;
  return l;
}


static gboolean update_proc_state(unsigned *ppid, pinfo *p, int delete_key) {
  if (p->update_stats_decnt == 0) {
    glibtop_proc_time time;
    p->time.flags = 0;
    if (p->locked < 0) return FALSE;
    if (p->update_cnt == 0 ||
	(p->update_cnt % 10) == 4) {
      /* why updating more than once ? because when the process forks,
	 it keeps its name, and when the forked process does an 'exec' its
	 name changes, hence wmforkplop missed that when the exec was 
	 delayed */
      glibtop_get_proc_state(&p->state, p->pid);
      glibtop_get_proc_uid(&p->uid, p->pid);
    }
    glibtop_get_proc_time(&time, p->pid);
    if (time.flags == 0) { /* process died */ 
      if (p->parent_aware != -2) { /* removal from child list of parent */
        int pp = p->uid.ppid;
        pinfo *daddy = (pinfo*)g_hash_table_lookup(proc_hash, &pp);
        if (daddy) 
          daddy->childs = g_list_remove(daddy->childs, p);
        p->parent_aware = -2;
        p->death_tics = app->tics_now;
      }
      if (!p->locked) {
        BLAHBLAH(1,printf("DEATH of %d (%s) [delete_key(%p)=%d]\n", *ppid, p->state.cmd,ppid,delete_key));
        if (delete_key) g_free(ppid); 
        pinfo_destroy(p); 
        return TRUE;
      } else p->locked = -1; /* -1 => process marked as dead, will be delete as soon as possible */
    } else {
      //printf("%s[%d] %d\n", p->state.cmd, p->update_cnt, p->smoothness);
      if (p->parent_aware == +1) { /* must add to child list ? */
        int pp = p->uid.ppid;
        pinfo *daddy = (pinfo*)g_hash_table_lookup(proc_hash, &pp);
        if (daddy)
          daddy->childs = g_list_insert(daddy->childs, p, 0);
        p->parent_aware = +2;
      }
      p->time = time;
      if ((p->update_cnt % 10 ) == 0 || p == app->single_pid_mode) {
        glibtop_get_proc_mem(&p->mem, p->pid);
        glibtop_get_proc_kernel(&p->kernel, p->pid);
        logged_data_push(&p->lsize, p->mem.vsize);
        logged_data_push(&p->lfaults, p->kernel.min_flt+p->kernel.maj_flt);
      }
      logged_data_push(&p->lcpu, p->time.utime + p->time.stime);
      p->update_stats_decnt = p->smoothness;
      if (cpu_usage(p) > 0.01) {
        p->update_stats_decnt = 1; 
        p->smoothness = MAX((p->smoothness*2)/3,3);
      } else if (p->update_cnt > 2) {
        p->smoothness = MIN(p->smoothness+1, 20) + (rand() % 3);
      }
      /*if (strchr("wX",p->state.cmd[0]))
        printf("updated PID %5d [%s]: cpu = %4.1f (%5d), smoothness = %2d, decnt = %2d nb_stat=%d\n", 
               (int)p->pid, p->state.cmd, cpu_usage(p)*100., (int)p->time.utime, p->smoothness, p->update_stats_decnt, p->nb_slices);
      */
      //p->smoothness = 200;
    }
    p->update_cnt++;
  } else p->update_stats_decnt--;
  return FALSE;
}

static void record_new_pid(pid_t pid) {
  int *key;
  pinfo *p = proc_hash_new_pid(pid, &key);
  //static int cnt=0;
  if (update_proc_state((unsigned*)key, p, 0) == TRUE) { /* if it returned TRUE, we have to
                                              remove the pinfo from proc_hash,
                                              the process died just after
                                              having been detected.. nasty race
                                              condition */
    g_hash_table_remove(proc_hash, key); g_free(key);
  }// else printf("new key: cnt = %d @ %p\n", ++cnt, key);
}

void update_top_stats() {
  static pid_t last_pid = 0;  
  //printf("update_top_stats(tic=%d)\n", (int)(app->tics_now));
  top_list_is_valid = 0; alpha_list_is_valid = 0;
  g_hash_table_foreach_remove(proc_hash, (GHRFunc)update_proc_state, (gpointer)1);
  //printf("check_ for new procs\n");
  if (last_pid != last_pid_new) {
    glibtop_proclist l;
    unsigned i;
    unsigned *ptr;
    if (last_pid_new - last_pid < 10 && last_pid) {
      for (i = last_pid; i <= (unsigned)last_pid_new; ++i) {
        ptr = glibtop_get_proclist(&l, GLIBTOP_KERN_PROC_PID, i);
        if (l.number == 1) record_new_pid(ptr[i]);
        if (ptr) g_free(ptr);
      }
    } else {
      ptr = glibtop_get_proclist (&l, GLIBTOP_KERN_PROC_ALL, 0);
      for (i = 0; i < l.number; ++i) {
        if (!proc_hash_find_pid(ptr[i]))
          record_new_pid(ptr[i]);
      }
      g_free(ptr);
    }
  }
}

static void do_kill_all_(pid_t *pid UNUSED, pinfo *p, const char *cmd) {
  if (strcmp(p->state.cmd, cmd) == 0) {
    BLAHBLAH(1,printf("KILLALL : %d [%s]\n", p->pid, p->state.cmd));
    kill(p->pid, SIGTERM);
  }
}

void do_kill_all(const char *cmd) {  
  g_hash_table_foreach(proc_hash, (GHFunc)do_kill_all_, (void*)cmd);
}

void update_stats() {
  update_fork_stats();
  if (!Prefs.disable_top_list) 
    update_top_stats();
}

void init_stats() {
  glibtop_init();
  proc_hash = g_hash_table_new(g_int_hash,g_int_equal);
  update_fork_stats();
}

