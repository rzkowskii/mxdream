/**
 * $Id$
 *
 * Core Cocoa-based user interface
 *
 * Copyright (c) 2008 Nathan Keynes.
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

#include <AppKit/AppKit.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "lxdream.h"
#include "dream.h"
#include "dreamcast.h"
#include "config.h"
#include "lxpaths.h"
#include "display.h"
#include "gui.h"
#include "gdrom/gdrom.h"
#include "gdlist.h"
#include "loader.h"
#include "cocoaui/cocoaui.h"
#include "drivers/video_nsgl.h"
#include <dispatch/dispatch.h>
#include <os/activity.h>

void cocoa_gui_update( void );
void cocoa_gui_start( void );
void cocoa_gui_stop( void );
uint32_t cocoa_gui_run_slice( uint32_t nanosecs );

struct dreamcast_module cocoa_gui_module = { "gui", NULL,
        cocoa_gui_update, 
        cocoa_gui_start, 
        cocoa_gui_run_slice, 
        cocoa_gui_stop, 
        NULL, NULL };

/**
 * Count of running nanoseconds - used to cut back on the GUI runtime
 */
static uint32_t cocoa_gui_nanos = 0;
static uint32_t cocoa_gui_ticks = 0;
static struct timeval cocoa_gui_lasttv;
static BOOL cocoa_gui_autorun = NO;
static BOOL cocoa_gui_is_running = NO;
static id cocoa_user_initiated_activity = nil; /* NSProcessInfo activity token */
static LxdreamMainWindow *mainWindow = NULL;
/* Configurable GUI tick and throttled status update period */
static uint32_t cocoa_gui_tick_period = GUI_TICK_PERIOD; /* ns */
static uint32_t cocoa_gui_status_accum_ns = 0;
static const uint32_t cocoa_gui_status_period_ns = 250000000; /* ~4 Hz */

/* Apply runtime video settings (integer scale + internal scale) when prefs change */
void cocoa_apply_video_runtime_toggles(void)
{
    extern struct lxdream_config_group * lxdream_get_config_group( int group );
    extern int pvr2_get_internal_scale_percent(void);
    extern void pvr2_set_internal_scale_percent(int percent);
    struct lxdream_config_group *vid = lxdream_get_config_group(4/*CONFIG_GROUP_VIDEO*/);
    if( vid ) {
        /* Internal render scale percent */
        const gchar *scale = vid->params[2/*internal_scale_percent*/].value;
        if( scale && *scale ) {
            int v = atoi((const char*)scale);
            pvr2_set_internal_scale_percent(v);
        }
        /* Integer scale toggle is read during size setup; trigger update */
        extern void video_nsgl_update(void);
        video_nsgl_update();
    }
}

/* Cadence logging under load */
static uint64_t cocoa_gui_cadence_accum_ns = 0; /* Sum of measured tick deltas */
static uint32_t cocoa_gui_cadence_ticks = 0;    /* Num ticks in the current window */
static uint64_t cocoa_gui_cadence_window_ns = 0;/* Window accumulator (~1s) */
static struct timeval cocoa_gui_cadence_lasttv; /* Timestamp of last tick */
static gboolean cocoa_gui_cadence_inited = FALSE;

@interface NSApplication (PrivateAdditions)
- (void) setAppleMenu:(NSMenu *)aMenu;
@end

gboolean cocoa_gui_disc_changed( cdrom_disc_t disc, const gchar *disc_name, void *user_data )
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    LxdreamMainWindow *window = (LxdreamMainWindow *)user_data;
    [window updateTitle];
    [pool release];
    return TRUE;
}

/**
 * Produces the menu title by looking the text up in gettext, removing any
 * underscores, and returning the result as an NSString.
 */
static NSString *NSMENU_( const char *text )
{
    const char *s = gettext(text);
    char buf[strlen(s)+1];
    char *d = buf;

    while( *s != '\0' ) {
        if( *s != '_' ) {
            *d++ = *s;
        }
        s++;
    }
    *d = '\0';
    
    return [NSString stringWithUTF8String: buf];
}

static void cocoa_gui_create_menu(void)
{
    int i;
    NSMenu *appleMenu, *services;
    NSMenuItem *menuItem;
    NSString *title;
    NSString *appName;

    appName = @"Mxdream";
    appleMenu = [[NSMenu alloc] initWithTitle:@""];

    /* Add menu items */
    title = [@"About " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(about_action:) keyEquivalent:@""];
    
    [appleMenu addItem:[NSMenuItem separatorItem]];
    [appleMenu addItemWithTitle: NSMENU_("_Preferences...") action:@selector(preferences_action:) keyEquivalent:@","];

    // Services Menu
    [appleMenu addItem:[NSMenuItem separatorItem]];
    services = [[[NSMenu alloc] init] autorelease];
    [appleMenu addItemWithTitle: NS_("Services") action:nil keyEquivalent:@""];
    [appleMenu setSubmenu: services forItem: [appleMenu itemWithTitle: @"Services"]];

    // Hide AppName
    title = [@"Hide " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

    // Hide Others
    menuItem = (NSMenuItem *)[appleMenu addItemWithTitle:@"Hide Others" 
                              action:@selector(hideOtherApplications:) 
                              keyEquivalent:@"h"];
    [menuItem setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];

    // Show All
    [appleMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [appleMenu addItem:[NSMenuItem separatorItem]];

    // Quit AppName
    title = [@"Quit " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];

    /* Put menu into the menubar */
    menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil  keyEquivalent:@""];
    [menuItem setSubmenu: appleMenu];
    NSMenu *menu = [NSMenu new];
    [menu addItem: menuItem];

    NSMenu *gdromMenu = cocoa_gdrom_menu_new();

    NSMenu *quickStateMenu = [[NSMenu alloc] initWithTitle:NSMENU_("_Quick State")];
    int quickState = dreamcast_get_quick_state();
    for( i=0; i<=MAX_QUICK_STATE; i++ ) {
    	NSString *label = [NSString stringWithFormat: NSMENU_("State _%d"), i];
    	NSString *keyEquiv = [NSString stringWithFormat: @"%d", i];
    	menuItem = [[NSMenuItem alloc] initWithTitle: label action: @selector(quick_state_action:) keyEquivalent: keyEquiv];
    	[menuItem setTag: i];
    	if( i == quickState ) {
    	    [menuItem setState:NSOnState];
    	}
    	[quickStateMenu addItem: menuItem];
    }

    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle: NSMENU_("_File")];
    [fileMenu addItemWithTitle: NSMENU_("Load _Binary...") action: @selector(load_binary_action:) keyEquivalent: @"b"];
    [[fileMenu addItemWithTitle: NSMENU_("_GD-Rom") action: nil keyEquivalent: @""]
      setSubmenu: gdromMenu];
    [fileMenu addItem: [NSMenuItem separatorItem]];
    [[fileMenu addItemWithTitle: NSMENU_("_Reset") action: @selector(reset_action:) keyEquivalent: @"r"]
      setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];
    [fileMenu addItemWithTitle: NSMENU_("_Pause") action: @selector(pause_action:) keyEquivalent: @"p"];
    [fileMenu addItemWithTitle: NS_("Resume") action: @selector(run_action:) keyEquivalent: @"r"];
    [fileMenu addItem: [NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle: NSMENU_("L_oad State...") action: @selector(load_action:) keyEquivalent: @"o"];
    [fileMenu addItemWithTitle: NSMENU_("S_ave State...") action: @selector(save_action:) keyEquivalent: @"a"];
    menuItem = [[NSMenuItem alloc] initWithTitle:NSMENU_("Select _Quick State") action: nil keyEquivalent: @""];
    [fileMenu addItem: [NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle: NSMENU_("_Load Quick State") action: @selector(quick_load_action:) keyEquivalent: @"l"]; 
    [fileMenu addItemWithTitle: NSMENU_("_Save Quick State") action: @selector(quick_save_action:) keyEquivalent: @"s"];
    [menuItem setSubmenu: quickStateMenu];
    [fileMenu addItem: menuItem];
    [fileMenu addItem: [NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle: NSMENU_("_Full Screen...") action: @selector(fullscreen_action:) keyEquivalent: @"\r"];

    menuItem = [[NSMenuItem alloc] initWithTitle:NSMENU_("_File") action: nil keyEquivalent: @""];
    [menuItem setSubmenu: fileMenu];
    [menu addItem: menuItem];

    /* Tell the application object that this is now the application menu */
    [NSApp setMainMenu: menu];
    [NSApp setAppleMenu: appleMenu];
    [NSApp setServicesMenu: services];

    /* Finally give up our references to the objects */
    [appleMenu release];
    [menuItem release];
    [menu release];
}

@interface LxdreamDelegate : NSObject
@end

@implementation LxdreamDelegate
- (void)windowWillClose: (NSNotification *)notice
{
    dreamcast_shutdown();
    /* Flush any pending async config writes before exit */
    extern void lxdream_flush_config_saves(void);
    lxdream_flush_config_saves();
    exit(0);
}
- (void)windowDidBecomeMain: (NSNotification *)notice
{
    if( cocoa_gui_autorun ) {
        cocoa_gui_autorun = NO;
        gui_do_later(dreamcast_run);
    }
    /* Raise UI QoS when frontmost */
    if( [NSProcessInfo instancesRespondToSelector:@selector(beginActivityWithOptions:reason:)] ) {
        if( cocoa_user_initiated_activity == nil ) {
            id token = [[NSProcessInfo processInfo] beginActivityWithOptions:NSActivityUserInitiated reason:@"Emulation UI active"];
            /* Retain under MRC to ensure token outlives the autorelease pool */
            cocoa_user_initiated_activity = [token retain];
        }
    }
}
- (void)windowDidBecomeKey: (NSNotification *)notice
{
    display_set_focused( TRUE );
    /* Resume display link if using displaylink mode */
    const char *vs_env = getenv("LXDREAM_VSYNC_MODE");
    if( vs_env && (strcasecmp(vs_env, "displaylink") == 0 || strcasecmp(vs_env, "dl") == 0) ) {
        extern void nsgl_start_displaylink_if_needed(void);
        nsgl_start_displaylink_if_needed();
    }
}
- (void)windowDidResignKey: (NSNotification *)notice
{
    display_set_focused( FALSE );
    [mainWindow setIsGrabbed: NO];
    /* Allow system to throttle when not active */
    if( [NSProcessInfo instancesRespondToSelector:@selector(endActivity:)] ) {
        if( cocoa_user_initiated_activity != nil ) {
            [[NSProcessInfo processInfo] endActivity:cocoa_user_initiated_activity];
            [cocoa_user_initiated_activity release];
            cocoa_user_initiated_activity = nil;
        }
    }
    /* Stop display link when unfocused to avoid redundant sync work */
    const char *vs_env = getenv("LXDREAM_VSYNC_MODE");
    if( vs_env && (strcasecmp(vs_env, "displaylink") == 0 || strcasecmp(vs_env, "dl") == 0) ) {
        extern void video_nsgl_shutdown();
        /* We don't want to destroy context; just stop the link safely */
        extern void nsgl_stop_displaylink_if_running(void);
        nsgl_stop_displaylink_if_running();
    }
}
- (BOOL)application: (NSApplication *)app openFile: (NSString *)filename
{
    const gchar *cname = [filename UTF8String];
    ERROR err;
    if( file_load_magic(cname, FALSE, &err) != FILE_ERROR ) {
        // Queue up a run event
        gui_do_later(dreamcast_run);
        return YES;
    } else {
        return NO;
    }
    
}
- (void) about_action: (id)sender
{
    NSString *extraCredits = @"Richard Ziolkowski 2025 - 2045";
    NSString *copyrightLine = [NSString stringWithFormat:@"%@\n%@",
                               NS_(lxdream_copyright),
                               extraCredits];
    NSArray *keys = [NSArray arrayWithObjects: @"Version", @"Copyright", nil];
    NSArray *values = [NSArray arrayWithObjects: NS_(lxdream_full_version), copyrightLine,  nil];

    NSDictionary *options= [NSDictionary dictionaryWithObjects: values forKeys: keys];

    [NSApp orderFrontStandardAboutPanelWithOptions: options];
}
- (void) preferences_action: (id)sender
{
    cocoa_gui_show_preferences();
}
- (void) load_action: (id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    NSString *path = [NSString stringWithCString: get_gui_path(CONFIG_SAVE_PATH)];
    [panel setDirectoryURL:[NSURL fileURLWithPath:path isDirectory:YES]];
    [panel setAllowedFileTypes:[NSArray arrayWithObject:@"dst"]];
    [panel beginSheetModalForWindow: mainWindow completionHandler:^(NSModalResponse result){
        if( result == NSModalResponseOK ) {
            NSURL *url = [[panel URLs] firstObject];
            if( url ) {
                dreamcast_load_state( [[url path] UTF8String] );
                set_gui_path(CONFIG_SAVE_PATH, [[[url URLByDeletingLastPathComponent] path] UTF8String]);
            }
        }
    }];
}
- (void) save_action: (id)sender
{
    NSSavePanel *panel = [NSSavePanel savePanel];
    NSString *path = [NSString stringWithCString: get_gui_path(CONFIG_SAVE_PATH)];
    [panel setDirectoryURL:[NSURL fileURLWithPath:path isDirectory:YES]];
    [panel setAllowedFileTypes:[NSArray arrayWithObject:@"dst"]];
    [panel beginSheetModalForWindow: mainWindow completionHandler:^(NSModalResponse result){
        if( result == NSModalResponseOK ) {
            NSURL *url = [panel URL];
            if( url ) {
                dreamcast_save_state( [[url path] UTF8String] );
                set_gui_path(CONFIG_SAVE_PATH, [[[url URLByDeletingLastPathComponent] path] UTF8String]);
            }
        }
    }];
}
- (void) load_binary_action: (id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    NSString *path = [NSString stringWithCString: get_gui_path(CONFIG_DEFAULT_PATH)];
    [panel setDirectoryURL:[NSURL fileURLWithPath:path isDirectory:YES]];
    [panel beginSheetModalForWindow: mainWindow completionHandler:^(NSModalResponse result){
        if( result == NSModalResponseOK ) {
            NSURL *url = [[panel URLs] firstObject];
            if( url ) {
                ERROR err; gboolean ok = file_load_exec( [[url path] UTF8String], &err );
                set_gui_path(CONFIG_DEFAULT_PATH, [[[url URLByDeletingLastPathComponent] path] UTF8String]);
                if( !ok ) { ERROR( err.msg ); }
            }
        }
    }];
}
- (void) mount_action: (id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    NSString *path = [NSString stringWithCString: get_gui_path(CONFIG_DEFAULT_PATH)];
    [panel setDirectoryURL:[NSURL fileURLWithPath:path isDirectory:YES]];
    [panel beginSheetModalForWindow: mainWindow completionHandler:^(NSModalResponse result){
        if( result == NSModalResponseOK ) {
            NSURL *url = [[panel URLs] firstObject];
            if( url ) {
                ERROR err; gboolean ok = gdrom_mount_image( [[url path] UTF8String], &err );
                if( !ok ) { ERROR(err.msg); }
                set_gui_path(CONFIG_DEFAULT_PATH, [[[url URLByDeletingLastPathComponent] path] UTF8String]);
            }
        }
    }];
}
- (void) pause_action: (id)sender
{
    dreamcast_stop();
}

- (void) reset_action: (id)sender
{
    dreamcast_reset();
}
- (void) run_action: (id)sender
{
    if( !dreamcast_is_running() ) {
        gui_do_later(dreamcast_run);
    }
}
- (void) gdrom_list_action: (id)sender
{
    ERROR err;
    gboolean ok = gdrom_list_set_selection( [sender tag], &err );
    if( !ok ) {
        ERROR( err.msg );
    }
}
- (void) fullscreen_action: (id)sender
{
    [mainWindow setFullscreen: ![mainWindow isFullscreen]]; 
}
- (void) quick_state_action: (id)sender
{
    [[[sender menu] itemWithTag: dreamcast_get_quick_state()] setState: NSOffState ];
    [sender setState: NSOnState ];
    dreamcast_set_quick_state( [sender tag] );
}
- (void) quick_save_action: (id)sender
{
    dreamcast_quick_save();
}
- (void) quick_load_action: (id)sender
{
    dreamcast_quick_load();
}
@end


gboolean gui_parse_cmdline( int *argc, char **argv[] )
{
    /* If started from the finder, the first (and only) arg will look something like 
     * -psn_0_... - we want to remove this so that lxdream doesn't try to process it 
     * normally
     */
    if( *argc == 2 && strncmp((*argv)[1], "-psn_", 5) == 0 ) {
        *argc = 1;
    }
    return TRUE;
}

gboolean gui_init( gboolean withDebug, gboolean withFullscreen )
{
    dreamcast_register_module( &cocoa_gui_module );

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    /* Ensure we present a regular app with menu bar when launched from CLI */
    if( [NSApp respondsToSelector:@selector(setActivationPolicy:)] ) {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    }

    LxdreamDelegate *delegate = [[LxdreamDelegate alloc] init];
    [NSApp setDelegate: (id)delegate];
    NSString *iconFile = [[NSBundle mainBundle] pathForResource:@"mxdream" ofType:@"icns"];
    if( iconFile == nil ) iconFile = [[NSBundle mainBundle] pathForResource:@"mxdream" ofType:@"png"];
    if( iconFile == nil ) iconFile = [[NSBundle mainBundle] pathForResource:@"lxdream" ofType:@"png"];
    if( iconFile != nil ) {
        NSImage *iconImage = [[[NSImage alloc] initWithContentsOfFile: iconFile] autorelease];
        if( iconImage ) {
            [iconImage setName: @"NSApplicationIcon"];
            [NSApp setApplicationIconImage: iconImage];
        }
    }
    cocoa_gui_create_menu();
    mainWindow = cocoa_gui_create_main_window();
    [mainWindow makeKeyAndOrderFront: nil];
    [NSApp activateIgnoringOtherApps: YES];   

    /* Optional GUI tick override via env: LXDREAM_GUI_TICK_MS (eg, 5). Clamp 2..20ms */
    const char *tick_ms_env = getenv("LXDREAM_GUI_TICK_MS");
    if( tick_ms_env != NULL ) {
        long v = strtol(tick_ms_env, NULL, 10);
        if( v < 2 ) v = 2; if( v > 20 ) v = 20;
        cocoa_gui_tick_period = (uint32_t)v * 1000000U;
    }
    /* Default a little faster UI cadence for smoother menus when not set */
    if( tick_ms_env == NULL ) {
        cocoa_gui_tick_period = 5U * 1000000U; /* 5ms */
    }

    /* Initialize macOS game controller input if available */
    extern void gc_osx_init(void);
    gc_osx_init();

    register_gdrom_disc_change_hook( cocoa_gui_disc_changed, mainWindow );
    if( withFullscreen ) {
    	[mainWindow setFullscreen: YES];
    }
    [pool release];
    return TRUE;
}

void gui_main_loop( gboolean run )
{
    if( run ) {
        cocoa_gui_autorun = YES;
    }
    cocoa_gui_is_running = YES;
    [NSApp run];
    cocoa_gui_is_running = NO;
}

void gui_update_state(void)
{
    cocoa_gui_update();
}

void gui_set_use_grab( gboolean grab )
{
    [mainWindow setUseGrab: (grab ? YES : NO)];
}

gboolean gui_error_dialog( const char *msg, ... )
{
    if( cocoa_gui_is_running ) {
        NSString *error_string;

        va_list args;
        va_start(args, msg);
        error_string = [[NSString alloc] initWithFormat: [NSString stringWithCString: msg] arguments: args];
        NSAlert *alert = [[[NSAlert alloc] init] autorelease];
        [alert setMessageText: NS_("Error in Mxdream")];
        [alert setInformativeText: [NSString stringWithUTF8String:error_string]];
        [alert addButtonWithTitle:@"OK"];
        [alert beginSheetModalForWindow: mainWindow completionHandler:nil];
        va_end(args);
        return TRUE;
    } else {
        return FALSE;
    }
}

void gui_update_io_activity( io_activity_type io, gboolean active )
{

}


uint32_t cocoa_gui_run_slice( uint32_t nanosecs )
{
    NSEvent *event;
    NSAutoreleasePool *pool;

    cocoa_gui_nanos += nanosecs;
    if( cocoa_gui_nanos > cocoa_gui_tick_period ) {
        cocoa_gui_nanos -= cocoa_gui_tick_period;
        cocoa_gui_ticks ++;
        uint32_t current_period = cocoa_gui_ticks * cocoa_gui_tick_period;
        cocoa_gui_status_accum_ns += cocoa_gui_tick_period;

        /* Cadence logging: compute measured delta since last tick and log avg every ~1s */
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        if( !cocoa_gui_cadence_inited ) {
            cocoa_gui_cadence_lasttv = tv_now;
            cocoa_gui_cadence_inited = TRUE;
        } else {
            uint64_t dt_ns = (uint64_t)(tv_now.tv_sec - cocoa_gui_cadence_lasttv.tv_sec) * 1000000000ULL +
                             (uint64_t)(tv_now.tv_usec - cocoa_gui_cadence_lasttv.tv_usec) * 1000ULL;
            cocoa_gui_cadence_lasttv = tv_now;
            cocoa_gui_cadence_accum_ns += dt_ns;
            cocoa_gui_cadence_ticks++;
            cocoa_gui_cadence_window_ns += dt_ns;
            if( cocoa_gui_cadence_window_ns >= 1000000000ULL ) {
                if( cocoa_gui_cadence_ticks > 0 ) {
                    uint64_t avg_ns = cocoa_gui_cadence_accum_ns / cocoa_gui_cadence_ticks;
                    /* Log once per ~second to stderr for now; could route to status later */
                    fprintf(stderr, "[GUI] tick avg %.2f ms over %u ticks\n",
                            (double)avg_ns / 1.0e6, cocoa_gui_cadence_ticks);
                }
                cocoa_gui_cadence_accum_ns = 0;
                cocoa_gui_cadence_ticks = 0;
                cocoa_gui_cadence_window_ns = 0;
            }
        }

        // Run the event loop across common modes to keep menus/modals responsive
        pool = [NSAutoreleasePool new];
        NSDate *now = [NSDate dateWithTimeIntervalSinceNow:0];
        // Default mode
        while( (event = [NSApp nextEventMatchingMask: NSAnyEventMask untilDate: now
                         inMode: NSDefaultRunLoopMode dequeue: YES]) != nil ) {
            [NSApp sendEvent: event];
        }
        [[NSRunLoop currentRunLoop] runMode: NSDefaultRunLoopMode beforeDate: now];
        // Tracking mode (menus, sliders, etc)
        BOOL had_tracking_events = NO;
        now = [NSDate dateWithTimeIntervalSinceNow:0];
        while( (event = [NSApp nextEventMatchingMask: NSAnyEventMask untilDate: now
                         inMode: NSEventTrackingRunLoopMode dequeue: YES]) != nil ) {
            [NSApp sendEvent: event];
            had_tracking_events = YES;
        }
        [[NSRunLoop currentRunLoop] runMode: NSEventTrackingRunLoopMode beforeDate: now];
        // Modal panels (open/save dialogs)
        BOOL had_modal_events = NO;
        now = [NSDate dateWithTimeIntervalSinceNow:0];
        while( (event = [NSApp nextEventMatchingMask: NSAnyEventMask untilDate: now
                         inMode: NSModalPanelRunLoopMode dequeue: YES]) != nil ) {
            [NSApp sendEvent: event];
            had_modal_events = YES;
        }
        [[NSRunLoop currentRunLoop] runMode: NSModalPanelRunLoopMode beforeDate: now];
        [pool release];

        struct timeval tv;
        gettimeofday(&tv,NULL);
        uint32_t ns = ((tv.tv_sec - cocoa_gui_lasttv.tv_sec) * 1000000000) + 
        (tv.tv_usec - cocoa_gui_lasttv.tv_usec)*1000;
        if( (ns * 1.05) < current_period ) {
            // We've gotten ahead - sleep for a little bit
            struct timespec tv;
            tv.tv_sec = 0;
            tv.tv_nsec = current_period - ns;
            nanosleep(&tv, &tv);
        }

        /* Dynamically throttle status updates based on state:
         * - Running and active, no menus: ~4 Hz
         * - Interacting with menus/modals: ~2 Hz
         * - Paused or app inactive: ~1 Hz
         */
        uint32_t status_period_ns = cocoa_gui_status_period_ns; /* default ~4 Hz */
        if( ![NSApp isActive] || !dreamcast_is_running() ) {
            status_period_ns = 1000000000U; /* 1 Hz */
        } else if( had_tracking_events || had_modal_events || [NSApp modalWindow] != nil ) {
            status_period_ns = 500000000U; /* 2 Hz */
        }

        if( cocoa_gui_status_accum_ns >= status_period_ns ) {
            cocoa_gui_status_accum_ns = 0;
            if( dreamcast_is_running() ) {
                gchar buf[32];
                double speed = (float)( (double)current_period * 100.0 / ns );
                cocoa_gui_lasttv.tv_sec = tv.tv_sec;
                cocoa_gui_lasttv.tv_usec = tv.tv_usec;
                snprintf( buf, 32, _("Running (%2.4f%%)"), speed );
                [mainWindow setStatusText: buf];
            }
        }
    }
    return nanosecs;
}

void cocoa_gui_update( void )
{

}

void cocoa_gui_start( void )
{
    [mainWindow setRunning: YES];
    cocoa_gui_nanos = 0;
    gettimeofday(&cocoa_gui_lasttv,NULL);
}

void cocoa_gui_stop( void )
{
    [mainWindow setRunning: NO];
}

@interface DoLaterStub : NSObject
{
    do_later_callback_t func;
}
@end    

@implementation DoLaterStub
- (id) init: (do_later_callback_t)f
{
    [super init];
    func = f;
    return self;
}
- (void) do
{
    func();
}
@end

/**
 * Queue a dreamcast_run() to execute after the currently event(s)
 */
void gui_do_later( do_later_callback_t func )
{
    DoLaterStub *stub = [[[DoLaterStub alloc] init: func] autorelease]; 
    [[NSRunLoop currentRunLoop] performSelector: @selector(do) 
     target: stub argument: nil order: 1 
     modes: [NSArray arrayWithObject: NSDefaultRunLoopMode] ];
}

/*************************** Convenience methods ***************************/

NSImage *NSImage_new_from_framebuffer( frame_buffer_t buffer )
{
    NSBitmapImageRep *rep = 
        [[NSBitmapImageRep alloc] initWithBitmapDataPlanes: &buffer->data
         pixelsWide: buffer->width  pixelsHigh: buffer->height
         bitsPerSample: 8 samplesPerPixel: 3
         hasAlpha: NO isPlanar: NO
         colorSpaceName: NSDeviceRGBColorSpace  bitmapFormat: 0
         bytesPerRow: buffer->rowstride  bitsPerPixel: 24];

    NSImage *image = [[NSImage alloc] initWithSize: NSMakeSize(0.0,0.0)];
    [image addRepresentation: rep];
    return image;
}


NSTextField *cocoa_gui_add_label( NSView *parent, NSString *text, NSRect frame )
{
    NSTextField *label = [[NSTextField alloc] initWithFrame: frame];
    [label setStringValue: text];
    [label setBordered: NO];
    [label setDrawsBackground: NO];
    [label setEditable: NO];
    [label setAutoresizingMask: (NSViewMinYMargin|NSViewMaxXMargin)];
    if( parent != NULL ) {
        [parent addSubview: label];
    }
    return label;
}
