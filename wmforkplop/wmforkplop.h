#ifndef WMFORKPLOP_H
#define WMFORKPLOP_H
#ifdef GKRELLM
#include <gkrellm2/gkrellm.h>
#endif
#include "config.h"
#include "util.h"
#include "dockapp_imlib2.h"
#include "procstat.h"

typedef struct IO_op_lst {
  enum { OP_READ, OP_WRITE } op;
  int n; /* "magnitude" of operation (log2 of kb/s) */
  int i,j; /* location of the IOMatrix */
  struct IO_op_lst *next;
} IO_op_lst;

#define CMAPSZ 256
typedef struct colormap {
  DATA32 p[CMAPSZ];
} cmap;

typedef struct {
  int w,h;
  int * __restrict * __restrict v; /* dim w+2, h+4 */
  cmap cm;
  IO_op_lst *ops;
} IOMatrix;

typedef struct {
  DockImlib2 *dock;
  Imlib_Font smallfont;

  int fn_h, fn_w; /* height of smallfont */

  char *current_smallfont_name;
  //unsigned update_display_delay_ms;
  
  IOMatrix iom;

  pinfo *single_pid_mode;

  int displayed_hd_changed;
  int reshape_cnt;
  
  clock_t tics_now; /* number of tics since an arbtitrary point in the past */
  time_t  time_now; /* number of seconds since EPOCH */
  long    tics_per_sec;
  long    page_size;
  int     kill_mode;
  int     mx, my; /* mouse coordinates */
} App;

void reshape(int w, int h);
#ifdef GKRELLM
int forkplop_main(int w, int h, GdkDrawable *gkdrawable);
void gkrellm_forkplop_update(int update_options);
#endif
void init_fonts(App*);
DECL_GLOB_INIT(App *app, NULL);
#endif
