/** Simple present queue facade. Not compiled by default. */
#pragma once
#include "mxdream_patch_config.h"

#ifdef __cplusplus
extern "C" {
#endif

void present_q_init(void);
void present_q_push(mx_render_buffer_t rb);
mx_render_buffer_t present_q_choose_for_vblank(void);
bool present_q_is_60hz(void);

#ifdef __cplusplus
}
#endif


