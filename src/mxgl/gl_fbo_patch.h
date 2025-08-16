#pragma once
#include "mxdream_patch_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fence helpers compatible with existing gl_fbo.c behavior */
void mx_fbo_begin(mx_render_buffer_t rb);
void mx_fbo_end(mx_render_buffer_t rb);
bool mx_fbo_is_ready(const mx_render_buffer_t rb);
void mx_fbo_wait_and_consume(mx_render_buffer_t rb);

#ifdef __cplusplus
}
#endif


