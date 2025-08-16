/* Minimal wrapper around os_signpost with a safe fallback */
#include "profiler.h"
#include <stdio.h>
#include <stdatomic.h>

#if LXDREAM_HAS_SIGNPOST
static os_log_t g_log;
#endif

void profiler_init(void)
{
#if LXDREAM_HAS_SIGNPOST
    g_log = os_log_create("org.mxdream.mxdream", "perf");
#endif
}

os_signpost_id_t profiler_begin(const char *name)
{
#if LXDREAM_HAS_SIGNPOST
    os_signpost_id_t sid = os_signpost_id_generate(g_log);
    os_signpost_interval_begin(g_log, sid, "%{public}s", name);
    return sid;
#else
    (void)name; return 0;
#endif
}

void profiler_end(const char *name, os_signpost_id_t sid)
{
#if LXDREAM_HAS_SIGNPOST
    os_signpost_interval_end(g_log, sid, "%{public}s", name);
#else
    (void)name; (void)sid; 
#endif
}

void profiler_event(const char *name)
{
#if LXDREAM_HAS_SIGNPOST
    os_signpost_event_emit(g_log, OS_SIGNPOST_ID_EXCLUSIVE, "%{public}s", name);
#else
    (void)name;
#endif
}


