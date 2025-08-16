#ifndef LXDREAM_PROFILER_H
#define LXDREAM_PROFILER_H

#if defined(__has_include)
#  if __has_include(<os/signpost.h>) && (__STDC_VERSION__ >= 201112L)
#    include <os/signpost.h>
#    include <os/log.h>
#    define LXDREAM_HAS_SIGNPOST 1
#  else
#    define LXDREAM_HAS_SIGNPOST 0
typedef unsigned long os_signpost_id_t;
#  endif
#else
#  define LXDREAM_HAS_SIGNPOST 0
typedef unsigned long os_signpost_id_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

void profiler_init(void);
os_signpost_id_t profiler_begin(const char *name);
void profiler_end(const char *name, os_signpost_id_t sid);
void profiler_event(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* LXDREAM_PROFILER_H */


