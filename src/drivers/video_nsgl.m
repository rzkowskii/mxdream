/**
 * $Id$
 *
 * Cocoa (NSOpenGL) video driver
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

#include <AppKit/NSOpenGL.h>
#include <Foundation/NSAutoreleasePool.h>
#include <CoreVideo/CoreVideo.h>
#include <dispatch/dispatch.h>
#include <stdatomic.h>
#include <stdlib.h>
#include "drivers/video_nsgl.h"
#include "drivers/video_gl.h"
#include "pvr2/glutil.h"
#include "profiler.h"

static NSOpenGLContext *nsgl_context = nil;
static CVDisplayLinkRef nsgl_display_link = NULL;
static BOOL nsgl_use_displaylink_mode = NO;

static CVReturn nsgl_displaylink_callback( CVDisplayLinkRef displayLink,
                                           const CVTimeStamp *now,
                                           const CVTimeStamp *outputTime,
                                           CVOptionFlags flagsIn,
                                           CVOptionFlags *flagsOut,
                                           void *displayLinkContext )
{
    (void)displayLink; (void)now; (void)outputTime; (void)flagsIn; (void)flagsOut; (void)displayLinkContext;
    // Avoid spamming the main queue: coalesce to at most one flush per tick
    static atomic_flag enqueued = ATOMIC_FLAG_INIT;
    if( atomic_flag_test_and_set(&enqueued) ) {
        return kCVReturnSuccess; // already queued
    }
    dispatch_async(dispatch_get_main_queue(), ^{
        os_signpost_id_t sid = profiler_begin("displaylink");
        if( nsgl_context != nil ) {
            [nsgl_context makeCurrentContext];
            /* Only present if a new frame is ready (checked in pvr2) */
            extern gboolean pvr2_frame_is_dirty(void);
            extern void pvr2_mark_presented(void);
            if( pvr2_frame_is_dirty() ) {
                [nsgl_context flushBuffer];
                /* Optional: update window title with debug HUD */
                extern int pvr2_debug_hud_enabled(void);
                if( pvr2_debug_hud_enabled() ) {
                    extern const char* pvr2_debug_hud_string(void);
                    NSWindow* win = [[nsgl_context view] window];
                    if( win ) {
                        const char* s = pvr2_debug_hud_string();
                        if( s ) [win setTitle:[NSString stringWithUTF8String:s]];
                    }
                }
                pvr2_mark_presented();
            }
        }
        profiler_end("displaylink", sid);
        atomic_flag_clear(&enqueued);
    });
    return kCVReturnSuccess;
}

gboolean video_nsgl_init_driver( NSView *view, display_driver_t driver )
{
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    /* Default MSAA off; opt-in via config or env (env overrides) */
    int msaa_samples = 0;
    {
        extern const gchar *lxdream_get_global_config_value(int key);
        extern struct lxdream_config_group * lxdream_get_config_group( int group );
        const gchar *cfg = NULL;
        struct lxdream_config_group *vid = lxdream_get_config_group(4/*CONFIG_GROUP_VIDEO*/);
        if( vid != NULL ) {
            cfg = vid->params[0/*VIDEO_MSAA_SAMPLES*/].value;
        }
        if( cfg && *cfg ) {
            int v = atoi(cfg);
            if( v == 2 || v == 4 || v == 8 ) msaa_samples = v;
        }
        const char *msaa_env = getenv("LXDREAM_MSAA_SAMPLES");
        if( msaa_env != NULL ) {
            int v = atoi(msaa_env);
            if( v == 2 || v == 4 || v == 8 ) msaa_samples = v;
        }
    }
    NSOpenGLPixelFormatAttribute attributes[32];
    int ai = 0;
    attributes[ai++] = NSOpenGLPFAWindow;
    attributes[ai++] = NSOpenGLPFAAccelerated;
    attributes[ai++] = NSOpenGLPFADoubleBuffer;
    attributes[ai++] = NSOpenGLPFADepthSize; attributes[ai++] = (NSOpenGLPixelFormatAttribute)24;
    /* Request a core profile context to avoid deprecated fixed-function on macOS */
#ifdef NSOpenGLPFAOpenGLProfile
    attributes[ai++] = NSOpenGLPFAOpenGLProfile;
#ifdef NSOpenGLProfileVersion3_2Core
    attributes[ai++] = (NSOpenGLPixelFormatAttribute)NSOpenGLProfileVersion3_2Core;
#else
    /* Fallback to legacy profile only if 3.2 core is unavailable at compile time */
    attributes[ai++] = (NSOpenGLPixelFormatAttribute)NSOpenGLProfileVersionLegacy;
#endif
#endif
    if( msaa_samples > 0 ) {
        attributes[ai++] = NSOpenGLPFAMultisample;
        attributes[ai++] = NSOpenGLPFASampleBuffers; attributes[ai++] = (NSOpenGLPixelFormatAttribute)1;
        attributes[ai++] = NSOpenGLPFASamples; attributes[ai++] = (NSOpenGLPixelFormatAttribute)msaa_samples;
    }
    attributes[ai++] = (NSOpenGLPixelFormatAttribute)nil;

    NSOpenGLPixelFormat *pixelFormat =
        [[[NSOpenGLPixelFormat alloc] initWithAttributes: attributes] autorelease];
    if( pixelFormat == nil ) {
        /* Fallback without MSAA/accelerated hint if unavailable */
        NSOpenGLPixelFormatAttribute fallback[] = {
            NSOpenGLPFAWindow,
            NSOpenGLPFAAccelerated,
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)24,
            (NSOpenGLPixelFormatAttribute)nil };
        pixelFormat = [[[NSOpenGLPixelFormat alloc] initWithAttributes: fallback] autorelease];
    }
    nsgl_context = 
        [[NSOpenGLContext alloc] initWithFormat: pixelFormat shareContext: nil];
    [nsgl_context setView: view];
    [nsgl_context makeCurrentContext];
    /* Select one v-sync mechanism: default to displaylink on macOS for smoother pacing,
     * unless explicitly overridden via LXDREAM_VSYNC_MODE=swap|interval. */
    const char *vs_env = getenv("LXDREAM_VSYNC_MODE");
    if( vs_env == NULL ) {
        nsgl_use_displaylink_mode = YES; /* default */
    } else {
        nsgl_use_displaylink_mode = (strcasecmp(vs_env, "displaylink") == 0 || strcasecmp(vs_env, "dl") == 0) ? YES : NO;
    }
    if( nsgl_use_displaylink_mode ) {
        GLint zero = 0;
        [nsgl_context setValues:&zero forParameter:NSOpenGLCPSwapInterval];
        if( [view respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)] ) {
            [(id)view setWantsBestResolutionOpenGLSurface:YES];
        }
        if( nsgl_display_link == NULL ) {
            CVDisplayLinkCreateWithActiveCGDisplays(&nsgl_display_link);
            CVDisplayLinkSetOutputCallback(nsgl_display_link, nsgl_displaylink_callback, NULL);
            if( nsgl_context != nil ) {
                CGLContextObj cgl = (CGLContextObj)[nsgl_context CGLContextObj];
                CGLPixelFormatObj cglpf = (CGLPixelFormatObj)[[nsgl_context pixelFormat] CGLPixelFormatObj];
                CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(nsgl_display_link, cgl, cglpf);
            }
            CVDisplayLinkStart(nsgl_display_link);
        }
    } else {
        GLint one = 1;
        [nsgl_context setValues:&one forParameter:NSOpenGLCPSwapInterval];
        if( [view respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)] ) {
            [(id)view setWantsBestResolutionOpenGLSurface:YES];
        }
        if( nsgl_display_link != NULL ) {
            CVDisplayLinkStop(nsgl_display_link);
            CVDisplayLinkRelease(nsgl_display_link);
            nsgl_display_link = NULL;
        }
    }

    [pool release];
    driver->swap_buffers = video_nsgl_swap_buffers;
    driver->capabilities.has_gl = TRUE;
    driver->capabilities.depth_bits = 24;
    gl_init_driver(driver, TRUE);
    return TRUE;
}

void video_nsgl_update()
{
    if( nsgl_context != nil ) {
        [nsgl_context update];
    }
}

void video_nsgl_make_current()
{
    if( nsgl_context != nil ) {
        [nsgl_context makeCurrentContext];
    }
}

void video_nsgl_swap_buffers()
{
    /* In displaylink mode, presents are driven by the CVDisplayLink callback */
    if( nsgl_use_displaylink_mode ) {
        return;
    }
    if( nsgl_context != nil ) {
        [nsgl_context flushBuffer];
    }
}

void video_nsgl_shutdown()
{
    if( nsgl_context != nil ) {
        [NSOpenGLContext clearCurrentContext];
        [nsgl_context release];
        nsgl_context = nil;
    }
    if( nsgl_display_link != NULL ) {
        CVDisplayLinkStop(nsgl_display_link);
        CVDisplayLinkRelease(nsgl_display_link);
        nsgl_display_link = NULL;
    }
}

/* Public helpers to control displaylink without tearing down the GL context */
void nsgl_stop_displaylink_if_running(void)
{
    if( nsgl_display_link != NULL ) {
        CVDisplayLinkStop(nsgl_display_link);
        CVDisplayLinkRelease(nsgl_display_link);
        nsgl_display_link = NULL;
    }
}

void nsgl_start_displaylink_if_needed(void)
{
    if( !nsgl_use_displaylink_mode ) return;
    if( nsgl_display_link != NULL ) return;
    CVDisplayLinkCreateWithActiveCGDisplays(&nsgl_display_link);
    CVDisplayLinkSetOutputCallback(nsgl_display_link, nsgl_displaylink_callback, NULL);
    if( nsgl_context != nil ) {
        CGLContextObj cgl = (CGLContextObj)[nsgl_context CGLContextObj];
        CGLPixelFormatObj cglpf = (CGLPixelFormatObj)[[nsgl_context pixelFormat] CGLPixelFormatObj];
        CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(nsgl_display_link, cgl, cglpf);
    }
    CVDisplayLinkStart(nsgl_display_link);
}
