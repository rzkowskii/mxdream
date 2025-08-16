/**
 * Lightweight GL state cache and no-op guards for legacy OpenGL paths.
 *
 * The cache mirrors a subset of frequently mutated state. Call the wrappers
 * in place of raw GL calls where practical. This is intentionally minimal to
 * avoid correctness risks – per-texture parameters are NOT globally cached.
 */
#ifndef LXDREAM_GL_STATE_H
#define LXDREAM_GL_STATE_H

#include "pvr2/glutil.h" /* ensures GL headers and helpers are included */

#ifdef __cplusplus
extern "C" {
#endif

void gl_state_cache_init(void);
void gl_state_cache_reset(void);

void gl_state_cache_active_texture(GLenum texture);
void gl_state_cache_bind_texture(GLenum target, GLuint tex);
void gl_state_cache_blend_func(GLenum src, GLenum dst);
void gl_state_cache_depth_func(GLenum func);
void gl_state_cache_depth_mask(GLboolean mask);
void gl_state_cache_set_enabled(GLenum cap, GLboolean enable);

/* Optional wrappers for texture parameters to avoid redundant sets on hot path.
 * These only dedupe the most common parameters used by the engine. */
void gl_state_cache_tex_parameter_i(GLenum target, GLenum pname, GLint param);

#ifdef __cplusplus
}
#endif

#endif /* LXDREAM_GL_STATE_H */


