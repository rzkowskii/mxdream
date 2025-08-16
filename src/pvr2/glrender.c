/**
 * $Id$
 *
 * Standard OpenGL rendering engine. 
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
#include "pvr2/pvr2mmio.h"
#include "pvr2/glutil.h"
#include "pvr2/scene.h"
#include "pvr2/tileiter.h"
#include "pvr2/shaders.h"
#include "drivers/gl_state.h"
#include "profiler.h"
#include "mxgl/gl_state.h"
#include "pvr2/pvr2.h"

#ifdef APPLE_BUILD
#include "OpenGL/CGLCurrent.h"
#include "OpenGL/CGLMacro.h"
#include <dispatch/dispatch.h>

static CGLContextObj CGL_MACRO_CONTEXT;
#endif

#define IS_NONEMPTY_TILE_LIST(p) (IS_TILE_PTR(p) && ((*((uint32_t *)(pvr2_main_ram+(p))) >> 28) != 0x0F))

int pvr2_poly_depthmode[8] = { GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL,
        GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, 
        GL_ALWAYS };
int pvr2_poly_srcblend[8] = { 
        GL_ZERO, GL_ONE, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, 
        GL_ONE_MINUS_DST_ALPHA };
int pvr2_poly_dstblend[8] = {
        GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA,
        GL_ONE_MINUS_DST_ALPHA };

static gboolean have_shaders = FALSE;
static int currentTexId = -1;
/* Translucent pass needs depth test but no depth writes, regardless of per-poly flags */
static gboolean s_force_no_depth_write = FALSE;

static inline void bind_texture(int texid)
{
    if( currentTexId != texid ) {
        currentTexId = texid;
        gl_state_cache_active_texture(GL_TEXTURE0);
        gl_state_cache_bind_texture(GL_TEXTURE_2D, texid);
    }
}

/**
 * Clip the tile bounds to the clipping plane. 
 * @return TRUE if the tile was not clipped completely.
 */
static gboolean clip_tile_bounds( uint32_t *tile, uint32_t *clip )
{
    if( tile[0] < clip[0] ) tile[0] = clip[0];
    if( tile[1] > clip[1] ) tile[1] = clip[1];
    if( tile[2] < clip[2] ) tile[2] = clip[2];
    if( tile[3] > clip[3] ) tile[3] = clip[3];
    return tile[0] < tile[1] && tile[2] < tile[3];
}

/* Core-profile-safe rectangle helper for modifier flushes: draws a 2D quad
 * as a triangle strip using immediate vertex attribs via glVertexAttrib*.
 * Assumes the current shader expects in_vertex as vec3 and ignores colour/tex.
 * For fixed-function builds, the old glBegin path is preserved by the
 * #ifdef below.
 */
static void drawrect2d( uint32_t tile_bounds[], float z )
{
#ifdef HAVE_OPENGL_FIXEDFUNC
    glBegin( GL_TRIANGLE_STRIP );
    glVertex3f( tile_bounds[0], tile_bounds[2], z );
    glVertex3f( tile_bounds[1], tile_bounds[2], z );
    glVertex3f( tile_bounds[0], tile_bounds[3], z );
    glVertex3f( tile_bounds[1], tile_bounds[3], z );
    glEnd();
#else
    /* Use the PVR2 shader's vertex attribute setter to point at a small
       client-side array, then restore to the scene array afterwards. */
    static GLfloat rect_vertices[4][3];
    rect_vertices[0][0] = (GLfloat)tile_bounds[0]; rect_vertices[0][1] = (GLfloat)tile_bounds[2]; rect_vertices[0][2] = z;
    rect_vertices[1][0] = (GLfloat)tile_bounds[1]; rect_vertices[1][1] = (GLfloat)tile_bounds[2]; rect_vertices[1][2] = z;
    rect_vertices[2][0] = (GLfloat)tile_bounds[0]; rect_vertices[2][1] = (GLfloat)tile_bounds[3]; rect_vertices[2][2] = z;
    rect_vertices[3][0] = (GLfloat)tile_bounds[1]; rect_vertices[3][1] = (GLfloat)tile_bounds[3]; rect_vertices[3][2] = z;

    /* Point the shader's vertex attribute at our temporary quad */
    glsl_set_pvr2_shader_in_vertex_vec3_pointer(&rect_vertices[0][0], sizeof(rect_vertices[0]));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    /* Restore the vertex attribute to the scene array */
    glsl_set_pvr2_shader_in_vertex_vec3_pointer(&pvr2_scene.vertex_array[0].x, sizeof(struct vertex_struct));
#endif
}

/*
 * Load textures for all polygons in the scene.
 * If LXDREAM_TEX_THREADS>1 and APPLE_BUILD, we parallelize address/ID lookup
 * across a small GCD thread pool and then apply GL work on the main thread.
 * GL calls remain serialized on the current context.
 */
static void pvr2_scene_load_textures()
{
    texcache_begin_scene( MMIO_READ( PVR2, RENDER_PALETTE ) & 0x03,
                         (MMIO_READ( PVR2, RENDER_TEXSIZE ) & 0x003F) << 5 );

    int total = pvr2_scene.poly_count;
#if defined(APPLE_BUILD)
    int threads = 1;
    {
        const char *env = getenv("LXDREAM_TEX_THREADS");
        if( env != NULL ) {
            int v = atoi(env);
            if( v >= 1 && v <= 8 ) threads = v;
        }
    }
    if( threads <= 1 || total < 256 ) {
        int i;
        for( i=0; i<total; i++ ) {
            struct polygon_struct *poly = &pvr2_scene.poly_array[i];
            if( POLY1_TEXTURED(poly->context[0]) ) {
                poly->tex_id = texcache_get_texture( poly->context[1], poly->context[2] );
                if( poly->mod_vertex_index != -1 ) {
                    if( pvr2_scene.shadow_mode == SHADOW_FULL ) {
                        poly->mod_tex_id = texcache_get_texture( poly->context[3], poly->context[4] );
                    } else {
                        poly->mod_tex_id = poly->tex_id;
                    }
                }
            } else {
                poly->tex_id = 0;
                poly->mod_tex_id = 0;
            }
        }
    } else {
        /*
         * Parallelize by partitioning polygons; we still call texcache_get_texture
         * which ultimately does GL uploads. To keep GL serialized to the active
         * context, we enqueue per-partition work onto the main queue.
         */
        int chunk = (total + threads - 1)/threads;
        __block int remaining = threads;
        dispatch_group_t group = dispatch_group_create();
        for( int t=0; t<threads; t++ ) {
            int start = t*chunk;
            int end = start + chunk;
            if( start >= total ) break;
            if( end > total ) end = total;
            dispatch_group_async(group, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                /* Batch GL work for this chunk into a single main-thread sync to reduce overhead */
                typedef struct { struct polygon_struct *poly; uint32_t c1,c2,c3,c4; int mod_index; } TexReq;
                int req_cap = end - start;
                if( req_cap < 1 ) req_cap = 1;
                TexReq *req = (TexReq *)malloc(sizeof(TexReq) * (size_t)req_cap);
                int req_count = 0;
                for( int i=start; i<end; i++ ) {
                    struct polygon_struct *poly = &pvr2_scene.poly_array[i];
                    if( POLY1_TEXTURED(poly->context[0]) ) {
                        TexReq r; r.poly = poly; r.c1 = poly->context[1]; r.c2 = poly->context[2];
                        r.c3 = poly->context[3]; r.c4 = poly->context[4]; r.mod_index = poly->mod_vertex_index;
                        req[req_count++] = r;
                    } else {
                        poly->tex_id = 0;
                        poly->mod_tex_id = 0;
                    }
                }
                if( req_count > 0 ) {
                    dispatch_sync(dispatch_get_main_queue(), ^{
                        for( int j=0; j<req_count; j++ ) {
                            struct polygon_struct *poly = req[j].poly;
                            poly->tex_id = texcache_get_texture( req[j].c1, req[j].c2 );
                            if( req[j].mod_index != -1 ) {
                                if( pvr2_scene.shadow_mode == SHADOW_FULL ) {
                                    poly->mod_tex_id = texcache_get_texture( req[j].c3, req[j].c4 );
                                } else {
                                    poly->mod_tex_id = poly->tex_id;
                                }
                            }
                        }
                    });
                }
                free(req);
            });
        }
        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    }
#else
    {
        int i;
        for( i=0; i<total; i++ ) {
            struct polygon_struct *poly = &pvr2_scene.poly_array[i];
            if( POLY1_TEXTURED(poly->context[0]) ) {
                poly->tex_id = texcache_get_texture( poly->context[1], poly->context[2] );
                if( poly->mod_vertex_index != -1 ) {
                    if( pvr2_scene.shadow_mode == SHADOW_FULL ) {
                        poly->mod_tex_id = texcache_get_texture( poly->context[3], poly->context[4] );
                    } else {
                        poly->mod_tex_id = poly->tex_id;
                    }
                }
            } else {
                poly->tex_id = 0;
                poly->mod_tex_id = 0;
            }
        }
    }
#endif
}


/**
 * Once-off call to setup the OpenGL context.
 */
void pvr2_setup_gl_context()
{
    have_shaders = display_driver->capabilities.has_sl;
#ifdef APPLE_BUILD
    CGL_MACRO_CONTEXT = CGLGetCurrentContext();
#endif
    gls_init();
    texcache_gl_init(); // Allocate texture IDs

    /* Global settings */
    glDisable( GL_CULL_FACE );
    gl_state_cache_set_enabled(GL_BLEND, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

#ifdef HAVE_OPENGL_CLAMP_COLOR
    if( isGLExtensionSupported("GL_ARB_color_buffer_float") ) {
        glClampColorARB(GL_CLAMP_VERTEX_COLOR_ARB, GL_FALSE );
        glClampColorARB(GL_CLAMP_FRAGMENT_COLOR_ARB, GL_FALSE );
    }
#endif

#ifdef HAVE_OPENGL_FIXEDFUNC
    /* Setup defaults for perspective correction + matrices */
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
#endif


#ifdef HAVE_GLES2
    glClearDepthf(0);
#else
    glClearDepth(0);
#endif
    glClearStencil(0);
}

void pvr2_shutdown_gl_context()
{
    texcache_gl_shutdown();
    pvr2_destroy_render_buffers();
}

/**
 * Setup the basic context that's shared between normal and modified modes -
 * depth, culling
 */
static void render_set_base_context( uint32_t poly1, gboolean set_depth )
{
    if( set_depth ) {
        gl_state_cache_depth_func( POLY1_DEPTH_MODE(poly1) );
    }
    if( s_force_no_depth_write ) {
        gl_state_cache_depth_mask( GL_FALSE );
    } else {
        gl_state_cache_depth_mask( POLY1_DEPTH_WRITE(poly1) ? GL_TRUE : GL_FALSE );
    }
}

/**
 * Setup the texture/shading settings (TSP) which vary between mod/unmod modes.
 */
static void render_set_tsp_context( uint32_t poly1, uint32_t poly2 )
{
#ifdef HAVE_OPENGL_FIXEDFUNC
    glShadeModel( POLY1_SHADE_MODEL(poly1) );

    if( !have_shaders ) {
        if( POLY1_TEXTURED(poly1) ) {
            if( POLY2_TEX_BLEND(poly2) == 2 )
                glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
            else
                glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
   
         }

         switch( POLY2_FOG_MODE(poly2) ) {
         case PVR2_POLY_FOG_LOOKUP:
             glFogfv( GL_FOG_COLOR, pvr2_scene.fog_lut_colour );
             break;
         case PVR2_POLY_FOG_VERTEX:
             glFogfv( GL_FOG_COLOR, pvr2_scene.fog_vert_colour );
             break;
         }
     }
#endif

     int srcblend = POLY2_SRC_BLEND(poly2);
     int destblend = POLY2_DEST_BLEND(poly2);
     gl_state_cache_blend_func( srcblend, destblend );

     if( POLY2_SRC_BLEND_TARGET(poly2) || POLY2_DEST_BLEND_TARGET(poly2) ) {
         WARN( "Accumulation buffer not supported" );
     }   
}

/**
 * Setup the GL context for the supplied polygon context.
 * @param context pointer to 3 or 5 words of polygon context
 * @param depth_mode force depth mode, or 0 to use the polygon's
 * depth mode.
 */
static void render_set_context( uint32_t *context, gboolean set_depth )
{
    render_set_base_context(context[0], set_depth);
    render_set_tsp_context(context[0],context[1]);
}

static inline void gl_draw_vertexes( struct polygon_struct *poly )
{
    do {
        glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index, poly->vertex_count);
        poly = poly->sub_next;
    } while( poly != NULL );
}

static inline void gl_draw_mod_vertexes( struct polygon_struct *poly )
{
    do {
        glDrawArrays(GL_TRIANGLE_STRIP, poly->mod_vertex_index, poly->vertex_count);
        poly = poly->sub_next;
    } while( poly != NULL );
}

static void gl_render_poly( struct polygon_struct *poly, gboolean set_depth)
{
    if( poly->vertex_count == 0 )
        return; /* Culled */

    /* If this poly uses a paletted texture, ensure palette strip is bound on unit 1 */
    if( POLY1_TEXTURED(poly->context[0]) && PVR2_TEX_IS_PALETTE(poly->context[2]) ) {
        extern GLuint texcache_get_palette_gltex(void);
        gl_state_cache_active_texture(GL_TEXTURE1);
        gl_state_cache_bind_texture(GL_TEXTURE_2D, texcache_get_palette_gltex());
        gl_state_cache_active_texture(GL_TEXTURE0);
    }

    bind_texture(poly->tex_id);
    if( poly->mod_vertex_index == -1 ) {
        render_set_context( poly->context, set_depth );
        gl_draw_vertexes(poly);
    }  else {
        gl_state_cache_set_enabled( GL_STENCIL_TEST, GL_TRUE );
        render_set_base_context( poly->context[0], set_depth );
        render_set_tsp_context( poly->context[0], poly->context[1] );
        glStencilFunc(GL_EQUAL, 0, 2);
        gl_draw_vertexes(poly);

        if( pvr2_scene.shadow_mode == SHADOW_FULL ) {
            bind_texture(poly->mod_tex_id);
            render_set_tsp_context( poly->context[0], poly->context[3] );
        }
        glStencilFunc(GL_EQUAL, 2, 2);
        gl_draw_mod_vertexes(poly);
        gl_state_cache_set_enabled( GL_STENCIL_TEST, GL_FALSE );
    }
}


static void gl_render_modifier_polygon( struct polygon_struct *poly, uint32_t tile_bounds[] )
{
    /* A bit of explanation:
     * In theory it works like this: generate a 1-bit stencil for each polygon
     * volume, and then AND or OR it against the overall 1-bit tile stencil at 
     * the end of the volume. 
     * 
     * The implementation here uses a 2-bit stencil buffer, where each volume
     * is drawn using only stencil bit 0, and then a 'flush' polygon is drawn
     * to update bit 1 accordingly and clear bit 0.
     * 
     * This could probably be more efficient, but at least it works correctly 
     * now :)
     */
    
    if( poly->vertex_count == 0 )
        return; /* Culled */

    gl_draw_vertexes(poly);


    
    int poly_type = POLY1_VOLUME_MODE(poly->context[0]);
    if( poly_type == PVR2_VOLUME_REGION0 ) {
        /* 00 => 00
         * 01 => 00
         * 10 => 10
         * 11 => 00
         */
        glStencilMask( 0x03 );
        glStencilFunc(GL_EQUAL, 0x02, 0x03);
        glStencilOp(GL_ZERO, GL_KEEP, GL_KEEP);
        gl_state_cache_set_enabled( GL_DEPTH_TEST, GL_FALSE );

        drawrect2d( tile_bounds, pvr2_scene.bounds[4] );
        
        gl_state_cache_set_enabled( GL_DEPTH_TEST, GL_TRUE );
        glStencilMask( 0x01 );
        glStencilFunc( GL_ALWAYS, 0, 1 );
        glStencilOp( GL_KEEP,GL_INVERT, GL_KEEP ); 
    } else if( poly_type == PVR2_VOLUME_REGION1 ) {
        /* This is harder with the standard stencil ops - do it in two passes
         * 00 => 00 | 00 => 10
         * 01 => 10 | 01 => 10
         * 10 => 10 | 10 => 00
         * 11 => 10 | 11 => 10
         */
        glStencilMask( 0x02 );
        glStencilOp( GL_INVERT, GL_INVERT, GL_INVERT );
        gl_state_cache_set_enabled( GL_DEPTH_TEST, GL_FALSE );
        
        drawrect2d( tile_bounds, pvr2_scene.bounds[4] );
        
        glStencilMask( 0x03 );
        glStencilFunc( GL_NOTEQUAL,0x02, 0x03);
        glStencilOp( GL_ZERO, GL_REPLACE, GL_REPLACE );
        
        drawrect2d( tile_bounds, pvr2_scene.bounds[4] );
        
        gl_state_cache_set_enabled( GL_DEPTH_TEST, GL_TRUE );
        glStencilMask( 0x01 );
        glStencilFunc( GL_ALWAYS, 0, 1 );
        glStencilOp( GL_KEEP,GL_INVERT, GL_KEEP );         
    }
}

static void gl_render_bkgnd( struct polygon_struct *poly )
{
    bind_texture(poly->tex_id);
    render_set_tsp_context( poly->context[0], poly->context[1] );
    gl_state_cache_set_enabled( GL_DEPTH_TEST, GL_FALSE );
    glBlendFunc( GL_ONE, GL_ZERO );
    gl_draw_vertexes(poly);
    gl_state_cache_set_enabled( GL_DEPTH_TEST, GL_TRUE );
}

void gl_render_triangle( struct polygon_struct *poly, int index )
{
    bind_texture(poly->tex_id);
    render_set_tsp_context( poly->context[0], poly->context[1] );
    glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index + index, 3 );

}

void gl_render_tilelist( pvraddr_t tile_entry, gboolean set_depth )
{
    tileentryiter list;

    FOREACH_TILEENTRY(list, tile_entry) {
        struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[TILEENTRYITER_POLYADDR(list)];
        if( poly != NULL ) {
            do {
                gl_render_poly(poly, set_depth);
                poly = poly->next;
            } while( list.strip_count-- > 0 );
        }
    }
}

/**
 * Render the tilelist with depthbuffer updates only.
 */
static void gl_render_tilelist_depthonly( pvraddr_t tile_entry )
{
    tileentryiter list;

    FOREACH_TILEENTRY(list, tile_entry) {
        struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[TILEENTRYITER_POLYADDR(list)];
        if( poly != NULL ) {
            do {
                render_set_base_context(poly->context[0],TRUE);
                gl_draw_vertexes(poly);
                poly = poly->next;
            } while( list.strip_count-- > 0 );
        }
    }
}

static void gl_render_modifier_tilelist( pvraddr_t tile_entry, uint32_t tile_bounds[] )
{
    tileentryiter list;

    FOREACH_TILEENTRY(list, tile_entry ) {
        struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[TILEENTRYITER_POLYADDR(list)];
        if( poly != NULL ) {
            do {
                gl_render_modifier_polygon( poly, tile_bounds );
                poly = poly->next;
            } while( list.strip_count-- > 0 );
        }
    }
}


#ifdef HAVE_OPENGL_FIXEDFUNC
void pvr2_scene_setup_fixed( GLfloat *viewMatrix )
{
    glLoadMatrixf(viewMatrix);
    glEnable( GL_DEPTH_TEST );
    
    glEnable( GL_FOG );
    glFogi(GL_FOG_COORDINATE_SOURCE_EXT, GL_FOG_COORDINATE_EXT);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.0);
    glFogf(GL_FOG_END, 1.0);

    glEnable( GL_ALPHA_TEST );
    glAlphaFunc( GL_GEQUAL, 0 );

    glEnable( GL_COLOR_SUM );

    glEnableClientState( GL_VERTEX_ARRAY );
    glEnableClientState( GL_COLOR_ARRAY );
    glEnableClientState( GL_TEXTURE_COORD_ARRAY );
    glEnableClientState( GL_SECONDARY_COLOR_ARRAY );
    glEnableClientState( GL_FOG_COORDINATE_ARRAY_EXT );

    /* Vertex array pointers */
    glVertexPointer(3, GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].x);
    glColorPointer(4, GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].rgba[0]);
    glTexCoordPointer(2, GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].u);
    glSecondaryColorPointerEXT(3, GL_FLOAT, sizeof(struct vertex_struct), pvr2_scene.vertex_array[0].offset_rgba );
    glFogCoordPointerEXT(GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].offset_rgba[3] );
}

void pvr2_scene_set_alpha_fixed( float alphaRef )
{
    glAlphaFunc( GL_GEQUAL, alphaRef );
}

void pvr2_scene_cleanup_fixed()
{
    glDisable( GL_COLOR_SUM );
    glDisable( GL_FOG );
    glDisable( GL_ALPHA_TEST );
    glDisable( GL_DEPTH_TEST );

    glDisableClientState( GL_VERTEX_ARRAY );
    glDisableClientState( GL_COLOR_ARRAY );
    glDisableClientState( GL_TEXTURE_COORD_ARRAY );
    glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
    glDisableClientState( GL_FOG_COORDINATE_ARRAY_EXT );

}
#else
void pvr2_scene_setup_fixed( GLfloat *viewMatrix )
{
}
void pvr2_scene_set_alpha_fixed( float alphaRef )
{
}
void pvr2_scene_cleanup_fixed()
{
}
#endif

void pvr2_scene_setup_shader( GLfloat *viewMatrix )
{
    glEnable( GL_DEPTH_TEST );

    glsl_use_pvr2_shader();
    glsl_set_pvr2_shader_view_matrix(viewMatrix);
    glsl_set_pvr2_shader_fog_colour1(pvr2_scene.fog_vert_colour);
    glsl_set_pvr2_shader_fog_colour2(pvr2_scene.fog_lut_colour);
    glsl_set_pvr2_shader_in_vertex_vec3_pointer(&pvr2_scene.vertex_array[0].x, sizeof(struct vertex_struct));
    glsl_set_pvr2_shader_in_colour_pointer(&pvr2_scene.vertex_array[0].rgba[0], sizeof(struct vertex_struct));
    glsl_set_pvr2_shader_in_colour2_pointer(&pvr2_scene.vertex_array[0].offset_rgba[0], sizeof(struct vertex_struct));
    glsl_set_pvr2_shader_in_texcoord_pointer(&pvr2_scene.vertex_array[0].u, sizeof(struct vertex_struct));
    glsl_set_pvr2_shader_alpha_ref(0.0);
    glsl_set_pvr2_shader_primary_texture(0);
    glsl_set_pvr2_shader_palette_texture(1);
}

void pvr2_scene_cleanup_shader( )
{
    glsl_clear_shader();

    glDisable( GL_DEPTH_TEST );
}

void pvr2_scene_set_alpha_shader( float alphaRef )
{
    glsl_set_pvr2_shader_alpha_ref(alphaRef);
}

/**
 * Render the currently defined scene in pvr2_scene
 */
void pvr2_scene_render( render_buffer_t buffer )
{
    /* Scene setup */
    struct timeval start_tv, tex_tv, end_tv;
    int i;
    GLfloat viewMatrix[16];
    uint32_t clip_bounds[4];


    gettimeofday(&start_tv, NULL);
    os_signpost_id_t sid_scene = profiler_begin("scene_render");
    display_driver->set_render_target(buffer);
    /* Ensure cached state and driver GL are in a known baseline each frame */
    gls_reset_frame();
    pvr2_check_palette_changed();
    pvr2_scene_load_textures();
    currentTexId = -1;

    gettimeofday( &tex_tv, NULL );
    uint32_t ms = (tex_tv.tv_sec - start_tv.tv_sec) * 1000 +
    (tex_tv.tv_usec - start_tv.tv_usec)/1000;
    DEBUG( "Texture load in %dms", ms );

    float alphaRef = ((float)(MMIO_READ(PVR2, RENDER_ALPHA_REF)&0xFF)+1)/256.0;
    float nearz = pvr2_scene.bounds[4];
    float farz = pvr2_scene.bounds[5];
    if( nearz == farz ) {
        farz*= 4.0;
    }

    /* Generate integer clip boundaries */
    for( i=0; i<4; i++ ) {
        clip_bounds[i] = (uint32_t)pvr2_scene.bounds[i];
    }

    defineOrthoMatrix(viewMatrix, pvr2_scene.buffer_width, pvr2_scene.buffer_height, -farz, -nearz);

    if( have_shaders ) {
        pvr2_scene_setup_shader(viewMatrix);
    } else {
        pvr2_scene_setup_fixed(viewMatrix);
    }


    /* Clear the buffer (FIXME: May not want always want to do this) */
    gl_state_cache_set_enabled( GL_SCISSOR_TEST, GL_FALSE );
    gl_state_cache_depth_mask( GL_TRUE );
    glStencilMask( 0x03 );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

    /* Render the background */
    gl_render_bkgnd( pvr2_scene.bkgnd_poly );

    gl_state_cache_set_enabled( GL_SCISSOR_TEST, GL_TRUE );
    /* Use state-cache wrapper to avoid mixed-state hazards */
    gl_state_cache_set_enabled( GL_TEXTURE_2D, GL_TRUE );

    struct tile_segment *segment;

#define FOREACH_SEGMENT(segment) \
    segment = pvr2_scene.segment_list; \
    do { \
        int tilex = SEGMENT_X(segment->control); \
        int tiley = SEGMENT_Y(segment->control); \
        \
        uint32_t tile_bounds[4] = { tilex << 5, (tilex+1)<<5, tiley<<5, (tiley+1)<<5 }; \
        if( !clip_tile_bounds(tile_bounds, clip_bounds) ) { \
            continue; \
        }
#define END_FOREACH_SEGMENT() \
    } while( !IS_LAST_SEGMENT(segment++) );
#define CLIP_TO_SEGMENT() \
    glScissor( tile_bounds[0], pvr2_scene.buffer_height-tile_bounds[3], tile_bounds[1]-tile_bounds[0], tile_bounds[3] - tile_bounds[2] )

    /* Build up the opaque stencil map */
    const char *light_en = getenv("LXDREAM_LIGHT_PIPELINE");
    gboolean light_pipeline = (light_en && atoi(light_en) != 0) ? TRUE : FALSE;
    if( display_driver->capabilities.stencil_bits >= 2 && !light_pipeline ) {
        glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
        FOREACH_SEGMENT(segment)
            if( IS_NONEMPTY_TILE_LIST(segment->opaquemod_ptr) ) {
                CLIP_TO_SEGMENT();
                gl_render_modifier_tilelist(segment->opaquemod_ptr, tile_bounds);
            }
        END_FOREACH_SEGMENT()
        gl_state_cache_depth_mask( GL_TRUE );
        glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
        gl_state_cache_set_enabled( GL_SCISSOR_TEST, GL_FALSE );
        glClear( GL_DEPTH_BUFFER_BIT );
        gl_state_cache_set_enabled( GL_SCISSOR_TEST, GL_TRUE );
        glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
    }

    /* Render the opaque polygons */
    extern void pvr2_debug_set_current_list(int list_id);
    extern void pvr2_debug_init(void);
    static int hud_inited = 0; if(!hud_inited){ pvr2_debug_init(); hud_inited=1; }
    pvr2_debug_set_current_list(0);
    /* Opaque: blending off, depth writes on, default depth func */
    gl_state_cache_set_enabled( GL_BLEND, GL_FALSE );
    gl_state_cache_depth_mask( GL_TRUE );
    gl_state_cache_depth_func( GL_LEQUAL );
    FOREACH_SEGMENT(segment)
        CLIP_TO_SEGMENT();
        gl_render_tilelist(segment->opaque_ptr,TRUE);
    END_FOREACH_SEGMENT()
    gl_state_cache_set_enabled( GL_STENCIL_TEST, GL_FALSE );

    /* Render the punch-out polygons */
    pvr2_debug_set_current_list(3);
    /* Punch-through: alpha-test only (configured below), blending off, depth writes on */
    gl_state_cache_set_enabled( GL_BLEND, GL_FALSE );
    gl_state_cache_depth_mask( GL_TRUE );
    if( have_shaders )
        pvr2_scene_set_alpha_shader(alphaRef);
    else
        pvr2_scene_set_alpha_fixed(alphaRef);
    gl_state_cache_depth_func(GL_GEQUAL);
    FOREACH_SEGMENT(segment)
        CLIP_TO_SEGMENT();
        if( !light_pipeline ) {
            gl_render_tilelist(segment->punchout_ptr, FALSE );
        }
    END_FOREACH_SEGMENT()
    if( have_shaders )
        pvr2_scene_set_alpha_shader(0.0);
    else
        pvr2_scene_set_alpha_fixed(0.0);
    /* Restore default depth func for subsequent passes */
    gl_state_cache_depth_func(GL_LEQUAL);

    /* Render the translucent polygons */
    pvr2_debug_set_current_list(2);
    /* Translucent: blending on (default blend), depth test on but no depth writes */
    gl_state_cache_set_enabled( GL_BLEND, GL_TRUE );
    gl_state_cache_blend_func( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    s_force_no_depth_write = TRUE;
    FOREACH_SEGMENT(segment)
        if( IS_NONEMPTY_TILE_LIST(segment->trans_ptr) ) {
            CLIP_TO_SEGMENT();
            if( pvr2_scene.sort_mode == SORT_NEVER || 
                    (pvr2_scene.sort_mode == SORT_TILEFLAG && (segment->control&SEGMENT_SORT_TRANS))) {
                if( !light_pipeline ) {
                    gl_render_tilelist(segment->trans_ptr, TRUE);
                }
            } else {
                if( !light_pipeline ) {
                    render_autosort_tile(segment->trans_ptr, RENDER_NORMAL );
                }
            }
        }
    END_FOREACH_SEGMENT()
    /* Reinstate depth writes post-translucent for any subsequent operations */
    s_force_no_depth_write = FALSE;

    gl_state_cache_set_enabled( GL_SCISSOR_TEST, GL_FALSE );

    if( have_shaders ) {
        pvr2_scene_cleanup_shader();
    } else {
        pvr2_scene_cleanup_fixed();
    }

    pvr2_scene_finished();

    gettimeofday( &end_tv, NULL );
    ms = (end_tv.tv_sec - tex_tv.tv_sec) * 1000 +
    (end_tv.tv_usec - tex_tv.tv_usec)/1000;
    DEBUG( "Scene render in %dms", ms );
    profiler_end("scene_render", sid_scene);
}
