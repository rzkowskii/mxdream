#include "pvr2/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int  g_enabled = 0;
static int  g_qdepth = 0, g_qcap = 0;
static int  g_list   = -1;
static char g_buf[128];

void pvr2_debug_init(void)
{
    const char* e = getenv("MXDREAM_DEBUG_HUD");
    g_enabled = (e && *e && *e != '0') ? 1 : 0;
}

int pvr2_debug_hud_enabled(void)
{
    return g_enabled;
}

void pvr2_debug_set_present_queue(int depth, int cap)
{
    g_qdepth = depth; g_qcap = cap;
}

void pvr2_debug_set_current_list(int list_id)
{
    g_list = list_id;
}

const char* pvr2_debug_hud_string(void)
{
    static const char* names[] = {"OPA","MOD","TRN","PT"};
    const char* ls = (g_list>=0 && g_list<4)? names[g_list] : "--";
    snprintf(g_buf, sizeof(g_buf), "mxdream — Q:%d/%d  List:%s", g_qdepth, g_qcap, ls);
    return g_buf;
}


