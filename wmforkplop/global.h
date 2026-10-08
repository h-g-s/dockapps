#ifndef GLOBAL_H
#define GLOBAL_H

//#include "config.h"

#ifdef __GNUC__
# define UNUSED __attribute((unused))
#else
# define UNUSED
#endif

#ifdef GLOBALS_HERE
# define DECL_GLOB(x) x;
# define DECL_GLOB_INIT(x,y) x = y
#else
# define DECL_GLOB(x) extern x
# define DECL_GLOB_INIT(x,y) extern x
#endif


#ifndef NO_BLAHBLAH
# define BLAHBLAH(n,x) if (Prefs.verbosity >= n) { x; fflush(stdout); }
#else
# define BLAHBLAH(n,x)
#endif

#ifndef NO_BITFIELDS
#  define BITFIELD(n) :n
#else
#  define BITFIELD(n) 
#endif 

#include "dockapp_imlib2.h"

typedef enum { AL_NONE=0, AL_LEFT=1, AL_HCENTER=2, AL_RIGHT=4, AL_TOP=8, AL_VCENTER=16,AL_BOTTOM=32} align_enum;

typedef struct {
  int verbosity;
  int disable_io_matrix;   /* disable background animation reflecting disk activity */
  int disable_top_list;
  char *smallfontname; /* ttf font name  + "/size" in pixels */
  unsigned iomatrix_colormap;
  unsigned hdlist_pos;
  float toplist_threshold;
  int toplist_remanence; /* a process remains on the toplist a certain amount of 
                            time after its cpu% has fallen below toplist_threshold */
  DockImlib2Prefs xprefs;
  unsigned update_stats_mult;
} structPrefs;

DECL_GLOB(structPrefs Prefs);



#define ASSIGN_STRING(a,v) { if (a) { free(a); a = NULL; } a = strdup(v); assert(a); }

#define TIMER_DELAY_MS 50

#endif

