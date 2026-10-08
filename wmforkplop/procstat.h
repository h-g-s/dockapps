#ifndef PROCSTAT_H
#define PROCSTAT_H
#include <glib.h>
#include <glibtop.h>
#include <glibtop/open.h>
#include <glibtop/close.h>

#include <glibtop/parameter.h>

#include <glibtop/union.h>
#include <glibtop/sysdeps.h>

typedef struct {
  int nb_slices, slice_cnt;
  guint64 *data, *tic;
  guint64 max_val;
} logged_data;

typedef struct {
  int pid; 
  GList *childs; /* list of pinfo * */
  int parent_aware; /* +1 => child must by added to parent list, 
                       +2 => child has been added
                       -1 => must be removed 
                       -2 => has been removed
                    */
  int smoothness;
  /* decnt for updating the general process statistics
     (cpu time, mem etc) */
  int update_stats_decnt;
  clock_t death_tics; /* seconds since boot */
  glibtop_proc_time time;
  glibtop_proc_state state;
  glibtop_proc_mem mem;
  glibtop_proc_kernel kernel;
  glibtop_proc_uid uid;
  logged_data lcpu, lsize, lfaults;
  /*struct {
    guint64 utime, tic;    
    } slices[NB_SLICES];*/
  int update_cnt; /* number of times the process info has been updated */
  int locked;
} pinfo;


void update_stats();

pinfo *proc_hash_find_pid(int pid);
float cpu_usage(pinfo *p);
gint64 vsize_increase(pinfo *p);
guint64 vsize_max_achieved(pinfo *p);
float faults_rate(pinfo *p);
double get_uptime();
double get_runtime(pinfo *p);
GList *get_top_processes();
GList *get_alpha_processes();
int get_fork_count();
int get_kill_count();
void do_kill_all(const char *cmd);
void init_stats();
#endif
