/**
 * mxgl GL state wrappers mapping to the existing minimal state cache.
 * These provide stable names for future migration without touching callers.
 */
#pragma once
#include "mxdream_patch_config.h"
#include "drivers/gl_state.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void gls_init(void) { gl_state_cache_init(); }
static inline void gls_reset_frame(void) { gl_state_cache_reset(); }
static inline void gls_reset_pass(void) { /* no-op placeholder */ }
static inline void gls_validate_and_resync(void) { /* no-op placeholder */ }

/* Convenience forwards to the existing state cache for common ops */
static inline void gls_active_texture(GLenum tex) { gl_state_cache_active_texture(tex); }
static inline void gls_bind_texture_2d(GLuint tex) { gl_state_cache_bind_texture(GL_TEXTURE_2D, tex); }
static inline void gls_set_blend(GLenum src, GLenum dst) { gl_state_cache_blend_func(src, dst); }
static inline void gls_set_depth_func(GLenum func) { gl_state_cache_depth_func(func); }
static inline void gls_set_depth_mask(GLboolean mask) { gl_state_cache_depth_mask(mask); }
static inline void gls_enable(GLenum cap) { gl_state_cache_set_enabled(cap, GL_TRUE); }
static inline void gls_disable(GLenum cap) { gl_state_cache_set_enabled(cap, GL_FALSE); }

#ifdef __cplusplus
}
#endif


