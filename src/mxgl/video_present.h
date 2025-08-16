#pragma once
#include "mxdream_patch_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fullscreen blit of a completed render buffer */
void video_gl_present_render_buffer(mx_render_buffer_t rb);

/* Safe defaults helper (optional) */
void mxdream_apply_safe_defaults(void);

#ifdef __cplusplus
}
#endif


