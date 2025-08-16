/**
 * $Id$
 *
 * Common GL code that doesn't depend on a specific implementation
 *
 * Copyright (c) 2005 Nathan Keynes.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <sys/time.h>

#include "display.h"
#include "pvr2/pvr2.h"
#include "pvr2/glutil.h"
#include "pvr2/shaders.h"
#include "drivers/video_gl.h"
#include "drivers/gl_state.h"

uint32_t video_width, video_height;
struct video_vertex {
    float x,y;
    float u,v;
    float r,g,b,a;
};

static struct video_box_t {
    float viewMatrix[16];
    struct video_vertex gap1[4];
    struct video_vertex gap2[4];
    struct video_vertex video_view[4];
    struct video_vertex invert_view[4];
} video_box;

void gl_set_video_size( uint32_t width, uint32_t height, int flipped )
{
    video_width = width;
    video_height = height;

    int x1=0,y1=0,x2=video_width,y2=video_height;
    int top = 0, bottom = 1;

    if( flipped ) {
        top = 1;
        bottom = 0;
    }

    /* Optional integer scaling for DC native aspect to avoid shimmering */
    int ah = video_width * 0.75;
    {
        extern struct lxdream_config_group * lxdream_get_config_group( int group );
        struct lxdream_config_group *vid = lxdream_get_config_group(4/*CONFIG_GROUP_VIDEO*/);
        if( vid && vid->params[1/*VIDEO_INTEGER_SCALE*/].value && atoi(vid->params[1/*VIDEO_INTEGER_SCALE*/].value) != 0 ) {
            /* Compute integer scale for 640x480 within current window */
            int max_scale_x = (int)(video_width / 640);
            int max_scale_y = (int)(video_height / 480);
            int scale = max_scale_x < max_scale_y ? max_scale_x : max_scale_y;
            if( scale < 1 ) scale = 1;
            int iw = 640 * scale;
            int ih = 480 * scale;
            x1 = (video_width - iw) / 2;
            x2 = x1 + iw;
            y1 = (video_height - ih) / 2;
            y2 = y1 + ih;
            /* Clear pillarbox/letterbox gaps */
            video_box.gap1[0].x = video_box.gap1[0].y = 0;
            video_box.gap1[1].x = x1; video_box.gap1[1].y = 0;
            video_box.gap1[2].x = x1; video_box.gap1[2].y = video_height;
            video_box.gap1[3].x = 0; video_box.gap1[3].y = video_height;
            video_box.gap2[0].x = x2; video_box.gap2[0].y = 0;
            video_box.gap2[1].x = video_width; video_box.gap2[1].y = 0;
            video_box.gap2[2].x = video_width; video_box.gap2[2].y = video_height;
            video_box.gap2[3].x = x2; video_box.gap2[3].y = video_height;
        }
    }

    if( ah > video_height ) {
        int w = (video_height/0.75);
        x1 = (video_width - w)/2;
        x2 -= x1;
        video_box.gap1[0].x = 0; video_box.gap1[0].y = 0;
        video_box.gap1[1].x = x1; video_box.gap1[1].y = 0;
        video_box.gap1[2].x = x1; video_box.gap1[2].y = video_height;
        video_box.gap1[3].x = 0; video_box.gap1[3].y = video_height;
        video_box.gap2[0].x = x2; video_box.gap2[0].y = 0;
        video_box.gap2[1].x = video_width; video_box.gap2[1].y = 0;
        video_box.gap2[2].x = video_width; video_box.gap2[2].y = video_height;
        video_box.gap2[3].x = x2; video_box.gap2[3].y = video_height;
    } else if( ah < video_height ) {
        y1 = (video_height - ah)/2;
        y2 -= y1;

        video_box.gap1[0].x = 0; video_box.gap1[0].y = 0;
        video_box.gap1[1].x = video_width; video_box.gap1[1].y = 0;
        video_box.gap1[2].x = video_width; video_box.gap1[2].y = y1;
        video_box.gap1[3].x = 0; video_box.gap1[3].y = y1;
        video_box.gap2[0].x = 0; video_box.gap2[0].y = y2;
        video_box.gap2[1].x = video_width; video_box.gap2[1].y = y2;
        video_box.gap2[2].x = video_width; video_box.gap2[2].y = video_height;
        video_box.gap2[3].x = 0; video_box.gap2[3].y = video_height;
    }

    video_box.video_view[0].x = x1; video_box.video_view[0].y = y1;
    video_box.video_view[0].u = 0; video_box.video_view[0].v = top;
    video_box.video_view[1].x = x2; video_box.video_view[1].y = y1;
    video_box.video_view[1].u = 1; video_box.video_view[1].v = top;
    video_box.video_view[2].x = x2; video_box.video_view[2].y = y2;
    video_box.video_view[2].u = 1; video_box.video_view[2].v = bottom;
    video_box.video_view[3].x = x1; video_box.video_view[3].y = y2;
    video_box.video_view[3].u = 0; video_box.video_view[3].v = bottom;

    memcpy( &video_box.invert_view, &video_box.video_view, sizeof(video_box.video_view) );
    video_box.invert_view[0].v = bottom; video_box.invert_view[1].v = bottom;
    video_box.invert_view[2].v = top; video_box.invert_view[3].v = top;

    defineOrthoMatrix(video_box.viewMatrix, video_width, video_height, 0, 65535);
}

#ifdef HAVE_OPENGL_FIXEDFUNC
/**
 * Setup the gl context for writes to the display output.
 */
void gl_framebuffer_setup()
{
    /* Even if compiled with fixed-function support, prefer the shader path to
     * remain compatible with macOS Core Profile. Mirror the non-fixedfunc path. */
    glViewport( 0, 0, video_width, video_height );
    gl_state_cache_blend_func( GL_ONE, GL_ZERO );
    glsl_use_basic_shader();
    glsl_set_basic_shader_view_matrix(video_box.viewMatrix);
    glsl_set_basic_shader_in_vertex_pointer(&video_box.gap1[0].x, sizeof(struct video_vertex));
    glsl_set_basic_shader_in_colour_pointer(&video_box.gap1[0].r, sizeof(struct video_vertex));
    glsl_set_basic_shader_in_texcoord_pointer(&video_box.gap1[0].u, sizeof(struct video_vertex));
    glsl_set_basic_shader_primary_texture(0);
    /* sRGB optional */
#ifdef GL_FRAMEBUFFER_SRGB
    static int srgb_inited = 0; static int srgb_on = 0;
    if( !srgb_inited ) { const char* e = getenv("MXDREAM_SRGB"); srgb_on = (e && *e && *e != '0') ? 1 : 0; srgb_inited = 1; }
    if( srgb_on ) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB);
#endif
}

void gl_framebuffer_cleanup()
{
    glsl_clear_shader();
}
#else
void gl_framebuffer_setup()
{
    glViewport( 0, 0, video_width, video_height );
    gl_state_cache_blend_func( GL_ONE, GL_ZERO );
    glsl_use_basic_shader();
    glsl_set_basic_shader_view_matrix(video_box.viewMatrix);
    glsl_set_basic_shader_in_vertex_pointer(&video_box.gap1[0].x, sizeof(struct video_vertex));
    glsl_set_basic_shader_in_colour_pointer(&video_box.gap1[0].r, sizeof(struct video_vertex));
    glsl_set_basic_shader_in_texcoord_pointer(&video_box.gap1[0].u, sizeof(struct video_vertex));
    glsl_set_basic_shader_primary_texture(0);
}

void gl_framebuffer_cleanup()
{
    glsl_clear_shader();
}
#endif

void gl_display_render_buffer( render_buffer_t buffer )
{
    gl_texture_window( buffer->width, buffer->height, buffer->buf_id, buffer->inverted );
}

/**
 * Convert window coordinates to dreamcast device coords (640x480) using the 
 * same viewable area as gl_texture_window.
 * If the coordinates are outside the viewable area, the result is -1,-1.
 */ 
void gl_window_to_system_coords( int *x, int *y )
{
    int x1=0,y1=0,x2=video_width,y2=video_height;

    int ah = video_width * 0.75;

    if( ah > video_height ) {
        int w = (video_height/0.75);
        x1 = (video_width - w)/2;
        x2 -= x1;
    } else if( ah < video_height ) {
        y1 = (video_height - ah)/2;
        y2 -= y1;
    }
    if( *x < x1 || *x >= x2 || *y < y1 || *y >= y2 ) {
        *x = -1;
        *y = -1;
    } else {
        *x = (*x - x1) * DISPLAY_WIDTH / (x2-x1);
        *y = (*y - y1) * DISPLAY_HEIGHT / (y2-y1);
    }
}

/* Present path must be core-profile safe: no GL_QUADS. We use an index
 * layout where vertices 0..3 = gap1 quad, 4..7 = gap2, 8..11 = video
 * quad, 12..15 = inverted video quad. Draw as two triangles each.
 */

void gl_texture_window( int width, int height, int tex_id, gboolean inverted )
{
    /* Set video box tex alpha to 1 */
    video_box.video_view[0].a = video_box.video_view[1].a = video_box.video_view[2].a = video_box.video_view[3].a = 1;
    video_box.invert_view[0].a = video_box.invert_view[1].a = video_box.invert_view[2].a = video_box.invert_view[3].a = 1;

    /* Reset display parameters */
    gl_framebuffer_setup();
    /* Draw pillar/letter-box gaps as two triangles each (0..3 and 4..7) */
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
    gl_state_cache_set_enabled(GL_TEXTURE_2D, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D,tex_id);
    /* Use nearest to preserve crisp DC pixels when upscaling */
    gl_state_cache_tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_state_cache_tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#ifndef HAVE_OPENGL_FIXEDFUNC
    /* Optionally replace final blit with GLSL upscale_simple (CAS-like) */
    {
        const char *fsr_en = getenv("LXDREAM_FSR");
        if( fsr_en && atoi(fsr_en) != 0 && display_driver->capabilities.has_sl ) {
            /* Switch to the upscaler program for the final textured quad */
            glsl_use_upscale_simple();
            glsl_set_upscale_simple_view_matrix(video_box.viewMatrix);
            glsl_set_upscale_simple_in_vertex_pointer(&video_box.gap1[0].x, sizeof(struct video_vertex));
            glsl_set_upscale_simple_in_texcoord_pointer(&video_box.gap1[0].u, sizeof(struct video_vertex));
            glsl_set_upscale_simple_primary_texture(0);
            float tx = width > 0 ? (1.0f / (float)width) : 0.0f;
            float ty = height > 0 ? (1.0f / (float)height) : 0.0f;
            float sharp = 0.2f;
            const char *sh = getenv("LXDREAM_FSR_SHARP");
            if( sh && *sh ) {
                int s = atoi(sh); if( s < 0 ) s = 0; if( s > 20 ) s = 20; sharp = (float)s / 20.0f;
            }
            GLfloat ts[2] = { tx, ty };
            glsl_set_upscale_simple_texel_size(ts);
            glsl_set_upscale_simple_sharp(sharp);
            glDrawArrays(GL_TRIANGLE_STRIP, inverted ? 12 : 8, 4);
        } else {
            glDrawArrays(GL_TRIANGLE_STRIP, inverted ? 12 : 8, 4);
        }
    }
#else
    glDrawArrays(GL_TRIANGLE_STRIP, inverted ? 12 : 8, 4);
#endif
    /* Optional simple sharpen (RCAS-lite) pass after blit, skipped if FSR GLSL used */
    const char *up_en = getenv("LXDREAM_UPSCALE_SIMPLE");
    const char *fsr_en = getenv("LXDREAM_FSR");
    if( up_en && atoi(up_en) != 0 && !(fsr_en && atoi(fsr_en) != 0) ) {
        /* Cheap unsharp-mask style: draw the quad again with slight offset samples and low weight */
        /* Note: Kept minimal to avoid heavy shader paths on deprecated GL */
        gl_state_cache_set_enabled(GL_BLEND, GL_TRUE);
        gl_state_cache_blend_func(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        gl_state_cache_tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl_state_cache_tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glColor4f(1.0f,1.0f,1.0f,0.15f);
        glDrawArrays(GL_TRIANGLE_STRIP, inverted ? 12 : 8, 4);
        gl_state_cache_set_enabled(GL_BLEND, GL_FALSE);
    }
    gl_state_cache_set_enabled(GL_TEXTURE_2D, GL_FALSE);
    gl_framebuffer_cleanup();
}

void gl_display_blank( uint32_t colour )
{
    /* Set the video_box background colour */
    video_box.video_view[0].r = ((float)(((colour >> 16) & 0xFF) + 1)) / 256.0;
    video_box.video_view[0].g = ((float)(((colour >> 8) & 0xFF) + 1)) / 256.0;
    video_box.video_view[0].b = ((float)((colour & 0xFF) + 1)) / 256.0;
    video_box.video_view[0].a = 0;
    memcpy( &video_box.video_view[1].r, &video_box.video_view[0].r, sizeof(float)*3 );
    memcpy( &video_box.video_view[2].r, &video_box.video_view[0].r, sizeof(float)*3 );
    memcpy( &video_box.video_view[3].r, &video_box.video_view[0].r, sizeof(float)*3 );

    /* And render */
    gl_framebuffer_setup();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
    glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
    gl_framebuffer_cleanup();
}


#ifdef HAVE_GLES2
/* Note: OpenGL ES only officialy supports glReadPixels for the RGBA32 format
 * (and doesn't necessarily support glPixelStore(GL_PACK_ROW_LENGTH) either.
 * As a result, we end up needed to do the format conversion ourselves.
 */

/**
 * Swizzle 32-bit RGBA to specified target format, truncating components where
 * necessary. Target may == source.
 * @param target destination buffer
 * @param Source buffer, which must be in 8-bit-per-component RGBA format, fully packed.
 * @param width width of image in pixels
 * @param height height of image in pixels
 * @param target_stride Stride of target buffer, in bytes
 * @param colour_format
 */
static void rgba32_to_target( unsigned char *target, const uint32_t *source, int width, int height, int target_stride, int colour_format )
{
    int x,y;

    if( target_stride == 0 )
        target_stride = width * colour_formats[colour_format].bpp;

    switch( colour_format ) {
    case COLFMT_BGRA1555:
        for( y=0; y<height; y++ ) {
            uint16_t *d = (uint16_t *)target;
            for( x=0; x<width; x++ ) {
                uint32_t v = *source++;
                *d++ = (uint16_t)( ((v & 0x80000000) >> 16) | ((v & 0x00F80000) >> 19) |
                        ((v & 0x0000F800)>>6) | ((v & 0x000000F8) << 7) );
            }
            target += target_stride;
        }
        break;
    case COLFMT_RGB565:
        for( y=0; y<height; y++ ) {
            uint16_t *d = (uint16_t *)target;
            for( x=0; x<width; x++ ) {
                uint32_t v = *source++;
                *d++ = (uint16_t)( ((v & 0x00F80000) >> 19) | ((v & 0x0000FC00) >> 5) | ((v & 0x000000F8)<<8) );
            }
            target += target_stride;
        }
        break;
    case COLFMT_BGRA4444:
        for( y=0; y<height; y++ ) {
            uint16_t *d = (uint16_t *)target;
            for( x=0; x<width; x++ ) {
                uint32_t v = *source++;
                *d++ = (uint16_t)( ((v & 0xF0000000) >> 16) | ((v & 0x00F00000) >> 20) |
                        ((v & 0x0000F000) >> 8) | ((v & 0x000000F0)<<4) );
            }
            target += target_stride;
        }
        break;
    case COLFMT_BGRA8888:
        for( y=0; y<height; y++ ) {
            uint32_t *d = (uint32_t *)target;
            for( x=0; x<width; x++ ) {
                uint32_t v = *source++;
                *d++ = (v & 0xFF00FF00) | ((v & 0x00FF0000) >> 16) | ((v & 0x000000FF)<<16);
            }
            target += target_stride;
        }
        break;
    case COLFMT_BGR0888:
        for( y=0; y<height; y++ ) {
            uint32_t *d = (uint32_t *)target;
            for( x=0; x<width; x++ ) {
                uint32_t v = *source++;
                *d++ = ((v & 0x00FF0000) >> 16) | (v & 0x0000FF00) | ((v & 0x000000FF)<<16);
            }
            target += target_stride;
        }
        break;
    case COLFMT_BGR888:
        for( y=0; y<height; y++ ) {
            uint8_t *d = (uint8_t *)target;
            for( x=0; x<width; x++ ) {
                uint32_t v = *source++;
                *d++ = (uint8_t)(v >> 16);
                *d++ = (uint8_t)(v >> 8);
                *d++ = (uint8_t)(v);
            }
            target += target_stride;
        }
        break;
    case COLFMT_RGB888:
        for( y=0; y<height; y++ ) {
            uint8_t *d = (uint8_t *)target;
            for( x=0; x<width; x++ ) {
                uint32_t v = *source++;
                *d++ = (uint8_t)(v);
                *d++ = (uint8_t)(v >> 8);
                *d++ = (uint8_t)(v >> 16);
            }
            target += target_stride;
        }
        break;
    default:
        assert( 0 && "Unsupported colour format" );
    }
}

/**
 * Convert data into an acceptable form for loading into an RGBA texture.
 */
static int target_to_rgba(  uint32_t *target, const unsigned char *source, int width, int height, int source_stride, int colour_format )
{
    int x,y;
    uint16_t *d;
    switch( colour_format ) {
    case COLFMT_BGRA1555:
        d = (uint16_t *)target;
        for( y=0; y<height; y++ ) {
            uint16_t *s = (uint16_t *)source;
            for( x=0; x<width; x++ ) {
                uint16_t v = *s++;
                *d++ = (v >> 15) | (v<<1);
            }
            source += source_stride;
        }
        return GL_UNSIGNED_SHORT_5_5_5_1;
        break;
    case COLFMT_RGB565:
        /* Need to expand to RGBA32 in order to have room for an alpha component */
        for( y=0; y<height; y++ ) {
            uint16_t *s = (uint16_t *)source;
            for( x=0; x<width; x++ ) {
                uint32_t v = (uint32_t)*s++;
                *target++ = ((v & 0xF800)>>8) | ((v & 0x07E0) <<5) | ((v & 0x001F) << 19) |
                        ((v & 0xE000) >> 13) | ((v &0x0600) >> 1) | ((v & 0x001C) << 14);
            }
            source += source_stride;
        }
        return GL_UNSIGNED_BYTE;
    case COLFMT_BGRA4444:
        d = (uint16_t *)target;
        for( y=0; y<height; y++ ) {
            uint16_t *s = (uint16_t *)source;
            for( x=0; x<width; x++ ) {
                uint16_t v = *s++;
                *d++ = (v >> 12) | (v<<4);
            }
            source += source_stride;
        }
        return GL_UNSIGNED_SHORT_4_4_4_4;
    case COLFMT_RGB888:
        for( y=0; y<height; y++ ) {
            uint8_t *s = (uint8_t *)source;
            for( x=0; x<width; x++ ) {
                *target++ = s[0] | (s[1]<<8) | (s[2]<<16);
                s += 3;
            }
            source += source_stride;
        }
        return GL_UNSIGNED_BYTE;
    case COLFMT_BGRA8888:
        for( y=0; y<height; y++ ) {
            uint32_t *s = (uint32_t *)source;
            for( x=0; x<width; x++ ) {
                uint32_t v = (uint32_t)*s++;
                *target++ = (v & 0xFF00FF00) | ((v & 0x00FF0000) >> 16) | ((v & 0x000000FF) << 16);
            }
            source += source_stride;
        }
        return GL_UNSIGNED_BYTE;
    case COLFMT_BGR0888:
        for( y=0; y<height; y++ ) {
            uint32_t *s = (uint32_t *)source;
            for( x=0; x<width; x++ ) {
                uint32_t v = (uint32_t)*s++;
                *target++ = (v & 0x0000FF00) | ((v & 0x00FF0000) >> 16) | ((v & 0x000000FF) << 16);
            }
            source += source_stride;
        }
        return GL_UNSIGNED_BYTE;
    case COLFMT_BGR888:
        for( y=0; y<height; y++ ) {
            uint8_t *s = (uint8_t *)source;
            for( x=0; x<width; x++ ) {
                *target++ = s[2] | (s[1]<<8) | (s[0]<<16);
                s += 3;
            }
            source += source_stride;
        }
        return GL_UNSIGNED_BYTE;
    default:
        assert( 0 && "Unsupported colour format" );
    }


}


gboolean gl_read_render_buffer( unsigned char *target, render_buffer_t buffer,
        int rowstride, int colour_format )
{
    if( colour_formats[colour_format].bpp == 4 && (rowstride == 0 || rowstride == buffer->width*4) ) {
        glReadPixels( 0, 0, buffer->width, buffer->height, GL_RGBA, GL_UNSIGNED_BYTE, target );
        rgba32_to_target( target, (uint32_t *)target, buffer->width, buffer->height, rowstride, colour_format );
    } else {
        int size = buffer->width * buffer->height;
        uint32_t tmp[size];

        glReadPixels( 0, 0, buffer->width, buffer->height, GL_RGBA, GL_UNSIGNED_BYTE, tmp );
        rgba32_to_target( target, tmp, buffer->width, buffer->height, rowstride, colour_format );
    }
    return TRUE;
}

void gl_frame_buffer_to_tex( frame_buffer_t frame, int tex_id )
{
    /* Optional PBO ring upload path (LXDREAM_PBO=1, LXDREAM_PBO_RING=N[2..6]) */
    static int pbo_enabled = -1;
    static GLuint pbo_ring[6];
    static int pbo_ring_count = 0;
    static int pbo_ring_index = 0;
    if( pbo_enabled < 0 ) {
        const char *env = getenv("LXDREAM_PBO");
        pbo_enabled = (env && atoi(env) != 0) ? 1 : 0;
        if( pbo_enabled ) {
            int rc = 3; /* default ring size */
            const char *r = getenv("LXDREAM_PBO_RING");
            if( r ) { int v = atoi(r); if( v >= 2 && v <= 6 ) rc = v; }
            pbo_ring_count = rc;
            glGenBuffers( (GLsizei)pbo_ring_count, pbo_ring );
        }
    }
    glBindTexture( GL_TEXTURE_2D, tex_id );
    if( pbo_enabled ) {
        size_t pixels = (size_t)frame->width * (size_t)frame->height;
        size_t bytes = pixels * 4; /* RGBA8 temp */
        GLuint pbo = pbo_ring[pbo_ring_index];
        pbo_ring_index = (pbo_ring_index + 1) % (pbo_ring_count > 0 ? pbo_ring_count : 1);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        /* Orphan previous contents, then map and convert into the buffer */
        glBufferData(GL_PIXEL_UNPACK_BUFFER, bytes, NULL, GL_STREAM_DRAW);
        void *ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        if( ptr ) {
            GLenum type = target_to_rgba( (uint32_t *)ptr, frame->data, frame->width, frame->height, frame->rowstride, frame->colour_format );
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            glTexSubImage2D( GL_TEXTURE_2D, 0, 0,0, frame->width, frame->height, GL_RGBA, type, 0 );
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    } else {
        int size = frame->width * frame->height;
        uint32_t tmp[size];
        GLenum type = target_to_rgba( tmp, frame->data, frame->width, frame->height, frame->rowstride, frame->colour_format );
        glTexSubImage2D( GL_TEXTURE_2D, 0, 0,0, frame->width, frame->height, GL_RGBA, type, tmp );
    }
    gl_check_error("gl_frame_buffer_to_tex");
}

#else
/**
 * Generic GL read_render_buffer. This function assumes that the caller
 * has already set the appropriate glReadBuffer(); in other words, unless
 * there's only one buffer this needs to be wrapped.
 */
gboolean gl_read_render_buffer( unsigned char *target, render_buffer_t buffer, 
                                int rowstride, int colour_format ) 
{
    /* Prefer fence-based completion when available */
    if( buffer->fence ) {
        GLenum wait = glClientWaitSync(buffer->fence, GL_SYNC_FLUSH_COMMANDS_BIT, 5*1000*1000);
        if( wait == GL_TIMEOUT_EXPIRED ) {
            glClientWaitSync(buffer->fence, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
        }
        glDeleteSync(buffer->fence);
        buffer->fence = 0;
    } else {
        /* Avoid blocking the whole GPU when fences are unavailable; at this
         * point we fall back to a conservative flush to ensure visibility. */
        glFlush();
    }
    GLenum type = colour_formats[colour_format].type;
    GLenum format = colour_formats[colour_format].format;
    // int line_size = buffer->width * colour_formats[colour_format].bpp;
    // int size = line_size * buffer->height;
    int glrowstride = (rowstride / colour_formats[colour_format].bpp) - buffer->width;
    glPixelStorei( GL_PACK_ROW_LENGTH, glrowstride );
    glReadPixels( 0, 0, buffer->width, buffer->height, format, type, target );
    glPixelStorei( GL_PACK_ROW_LENGTH, 0 );
    return TRUE;
}

void gl_frame_buffer_to_tex( frame_buffer_t frame, int tex_id )
{
    GLenum type = colour_formats[frame->colour_format].type;
    GLenum format = colour_formats[frame->colour_format].format;
    int bpp = colour_formats[frame->colour_format].bpp;
    int rowstride = (frame->rowstride / bpp) - frame->width;

    glPixelStorei( GL_UNPACK_ROW_LENGTH, rowstride );
    glBindTexture( GL_TEXTURE_2D, tex_id );
    glTexSubImage2DBGRA( 0, 0,0,
                     frame->width, frame->height, format, type, frame->data, FALSE );
    glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
}
#endif

void gl_load_frame_buffer( frame_buffer_t frame, render_buffer_t render )
{
    gl_frame_buffer_to_tex( frame, render->tex_id );
}


gboolean gl_init_driver( display_driver_t driver, gboolean need_fbo )
{
    /* Use framebuffer objects if available */
    if( gl_fbo_is_supported() ) {
        gl_fbo_init(driver);
    } else if( need_fbo ) {
        ERROR( "Framebuffer objects not supported - unable to construct an off-screen buffer" );
        return FALSE;
    }

    /* Use SL shaders if available */
    glsl_init(driver);
#ifndef HAVE_OPENGL_FIXEDFUNC
    if( !driver->has_sl ) { /* Shaders are required if we don't have fixed-functionality */
        gl_fbo_shutdown();
        return FALSE;
    }
#endif

    /* Use vertex arrays, VBOs, etc, if we have them */
    gl_vbo_init(driver);

    driver->capabilities.has_gl = TRUE;
    driver->capabilities.has_bgra = isGLBGRATextureSupported();
    return TRUE;
}

static gboolean video_gl_init();

/**
 * Minimal GL driver (assuming that the GL context is already set up externally)
 * This requires FBO support (since otherwise we have no way to get a render buffer)
 */
struct display_driver display_gl_driver = {
        "gl", N_("OpenGL driver"), video_gl_init, NULL,
        NULL, NULL, NULL,
        NULL, NULL, NULL, NULL,
        gl_load_frame_buffer, gl_display_render_buffer, gl_display_blank,
        NULL, gl_read_render_buffer, NULL, NULL
};

static gboolean video_gl_init()
{
    return gl_init_driver(&display_gl_driver, TRUE);
}
