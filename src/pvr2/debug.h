/** Optional lightweight debug HUD helpers (title-bar only). */
#ifndef LXDREAM_PVR2_DEBUG_H
#define LXDREAM_PVR2_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

void pvr2_debug_init(void);
void pvr2_debug_set_present_queue(int depth, int cap);
void pvr2_debug_set_current_list(int list_id);
const char* pvr2_debug_hud_string(void);
int  pvr2_debug_hud_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* LXDREAM_PVR2_DEBUG_H */


