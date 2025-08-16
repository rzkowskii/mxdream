#pragma once
/* Optional GL tripwire to discourage direct state manipulation when migrating
 * to cached wrappers. Enable by defining MX_TRIPWIRE_GL before inclusion. */
#include "mxdream_patch_config.h"

#if MX_TRIPWIRE_GL
#define glEnable(...)        MX_USE_gls_enable_instead
#define glDisable(...)       MX_USE_gls_disable_instead
#define glDepthMask(...)     MX_USE_gls_depth_mask_instead
#define glColorMask(...)     MX_USE_gls_color_mask_instead
#define glScissor(...)       MX_USE_gls_set_scissor_instead
#define glBlendFunc(...)     MX_USE_gls_set_blend_instead
#define glBlendFuncSeparate(...) MX_USE_gls_set_blend_instead
#define glActiveTexture(...) MX_USE_gls_active_texture_instead
#define glBindTexture(...)   MX_USE_gls_bind_texture_instead
#endif


