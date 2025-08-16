/** Minimal GL state cache implementation. */
#include <string.h>
#include <stdlib.h>
#include "drivers/gl_state.h"

typedef struct CachedState {
    GLenum activeTexture;
    GLuint boundTexture2D;
    GLenum blendSrc, blendDst;
    GLenum depthFunc;
    GLboolean depthMask;
    /* Caps */
    GLboolean blendEnabled;
    GLboolean depthTestEnabled;
    GLboolean stencilTestEnabled;
    GLboolean scissorTestEnabled;
    /* Limited texture parameter cache for GL_TEXTURE_2D */
    GLint tex2DMinFilter;
    GLint tex2DMagFilter;
    GLint tex2DWrapS;
    GLint tex2DWrapT;
    int enabled; /* global toggle via env */
} CachedState;

static CachedState s;

static int env_enabled(void)
{
    static int inited = 0;
    static int enabled = 1;
    if( !inited ) {
        const char *e = getenv("LXDREAM_GL_STATE_CACHE");
        enabled = (e == NULL || atoi(e) != 0) ? 1 : 0;
        inited = 1;
    }
    return enabled;
}

void gl_state_cache_init(void)
{
    memset(&s, 0, sizeof(s));
    s.activeTexture = GL_TEXTURE0;
    s.boundTexture2D = 0;
    s.blendSrc = GL_ONE; s.blendDst = GL_ZERO;
    s.depthFunc = GL_LESS; s.depthMask = GL_TRUE;
    s.blendEnabled = GL_FALSE;
    s.depthTestEnabled = GL_FALSE;
    s.stencilTestEnabled = GL_FALSE;
    s.scissorTestEnabled = GL_FALSE;
    s.tex2DMinFilter = -1;
    s.tex2DMagFilter = -1;
    s.tex2DWrapS = -1;
    s.tex2DWrapT = -1;
    s.enabled = env_enabled();
}

void gl_state_cache_reset(void)
{
    gl_state_cache_init();
}

void gl_state_cache_active_texture(GLenum texture)
{
    if( !s.enabled ) { glActiveTexture(texture); return; }
    if( s.activeTexture != texture ) {
        s.activeTexture = texture;
        glActiveTexture(texture);
    }
}

void gl_state_cache_bind_texture(GLenum target, GLuint tex)
{
    if( !s.enabled ) { glBindTexture(target, tex); return; }
    if( target == GL_TEXTURE_2D ) {
        if( s.boundTexture2D != tex ) {
            s.boundTexture2D = tex;
            glBindTexture(target, tex);
        }
    } else {
        glBindTexture(target, tex);
    }
}

void gl_state_cache_blend_func(GLenum src, GLenum dst)
{
    if( !s.enabled ) { glBlendFunc(src, dst); return; }
    if( s.blendSrc != src || s.blendDst != dst ) {
        s.blendSrc = src; s.blendDst = dst;
        glBlendFunc(src, dst);
    }
}

void gl_state_cache_depth_func(GLenum func)
{
    if( !s.enabled ) { glDepthFunc(func); return; }
    if( s.depthFunc != func ) {
        s.depthFunc = func;
        glDepthFunc(func);
    }
}

void gl_state_cache_depth_mask(GLboolean mask)
{
    if( !s.enabled ) { glDepthMask(mask); return; }
    if( s.depthMask != mask ) {
        s.depthMask = mask;
        glDepthMask(mask);
    }
}

static void set_cap(GLenum cap, GLboolean enable)
{
    if( enable ) glEnable(cap); else glDisable(cap);
}

void gl_state_cache_set_enabled(GLenum cap, GLboolean enable)
{
    if( !s.enabled ) { set_cap(cap, enable); return; }
    GLboolean *slot = NULL;
    switch(cap) {
        case GL_BLEND: slot = &s.blendEnabled; break;
        case GL_DEPTH_TEST: slot = &s.depthTestEnabled; break;
        case GL_STENCIL_TEST: slot = &s.stencilTestEnabled; break;
        case GL_SCISSOR_TEST: slot = &s.scissorTestEnabled; break;
        default: set_cap(cap, enable); return;
    }
    if( *slot != enable ) {
        *slot = enable;
        set_cap(cap, enable);
    }
}

void gl_state_cache_tex_parameter_i(GLenum target, GLenum pname, GLint param)
{
    if( !s.enabled ) { glTexParameteri(target, pname, param); return; }
    if( target != GL_TEXTURE_2D ) { glTexParameteri(target, pname, param); return; }
    GLint *slot = NULL;
    switch(pname) {
        case GL_TEXTURE_MIN_FILTER: slot = &s.tex2DMinFilter; break;
        case GL_TEXTURE_MAG_FILTER: slot = &s.tex2DMagFilter; break;
        case GL_TEXTURE_WRAP_S: slot = &s.tex2DWrapS; break;
        case GL_TEXTURE_WRAP_T: slot = &s.tex2DWrapT; break;
        default: glTexParameteri(target, pname, param); return;
    }
    if( *slot != param ) {
        *slot = param;
        glTexParameteri(target, pname, param);
    }
}


