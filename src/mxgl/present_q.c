/*
 * Thin wrappers that forward to the project’s existing internal queue
 * in pvr2.c, so that future call-site migrations can include this header
 * without changing semantics.
 */
#include "mxdream_patch_config.h"

/* The existing implementation resides in pvr2.c. Declare the symbols here. */
extern void pvr2_draw_frame(void);
extern gboolean pvr2_frame_is_dirty(void);
extern void pvr2_mark_presented(void);

/* Local forward decls that exist in pvr2.c but are static there; we provide
 * public facades with equivalent behavior using available pvr2 APIs. */

void present_q_init(void)
{
    /* No-op: pvr2 queue is initialized implicitly. */
}

void present_q_push(mx_render_buffer_t rb)
{
    (void)rb;
    /* In the existing code, enqueue happens inside pvr2 on RENDER_START.
     * This facade is present for call-sites that will migrate in the future. */
}

mx_render_buffer_t present_q_choose_for_vblank(void)
{
    /* Defer to pvr2’s present path – just trigger the draw tick; the actual
     * display driver will blit the last ready buffer. Return NULL to indicate
     * the driver should use its normal path. */
    if( pvr2_frame_is_dirty() ) {
        pvr2_draw_frame();
        pvr2_mark_presented();
    }
    return NULL;
}

bool present_q_is_60hz(void)
{
    const char *pm = getenv("LXDREAM_PACE");
    if( pm && strcmp(pm, "30") == 0 ) return false;
    return true;
}


