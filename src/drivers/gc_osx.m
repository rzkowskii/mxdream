/**
 * Minimal macOS GameController-based gamepad input driver
 */

#import <Foundation/Foundation.h>
#import <GameController/GameController.h>
#include <glib.h>
#include "display.h"

typedef struct gc_pad_driver {
    struct input_driver base;
    GCController *controller;
    guint buttonCount;
    guint axisCount; /* each axis contributes 2 keycodes (+/-) */
} *gc_pad_driver_t;

static GList *gc_drivers = NULL;
static guint gc_next_id = 0;

static uint16_t gc_resolve_keysym( struct input_driver *driver, const gchar *keysym )
{
    gc_pad_driver_t pad = (gc_pad_driver_t)driver;
    if( g_str_has_prefix(keysym, "Button") ) {
        unsigned long button = strtoul( keysym+6, NULL, 10 );
        if( button >= 1 && button <= pad->buttonCount ) return (uint16_t)button;
        return 0;
    }
    if( g_str_has_prefix(keysym, "Axis") ) {
        char *endptr;
        unsigned long axis = strtoul( keysym+4, &endptr, 10 );
        if( axis < 1 || axis > pad->axisCount ) return 0;
        int keycode = ((axis - 1) << 1) + pad->buttonCount + 1;
        return (*endptr == '-') ? (keycode + 1) : keycode;
    }
    return 0;
}

static gchar *gc_keysym_for_keycode( struct input_driver *driver, uint16_t keycode )
{
    gc_pad_driver_t pad = (gc_pad_driver_t)driver;
    if( keycode == 0 ) return NULL;
    if( keycode <= pad->buttonCount ) return g_strdup_printf("Button%u", keycode);
    if( keycode <= pad->buttonCount + pad->axisCount*2 ) {
        int axis = keycode - pad->buttonCount - 1;
        return (axis & 1) ? g_strdup_printf("Axis%d-", (axis>>1)+1)
                          : g_strdup_printf("Axis%d+", (axis>>1)+1);
    }
    return NULL;
}

static void gc_destroy( struct input_driver *driver )
{
    gc_pad_driver_t pad = (gc_pad_driver_t)driver;
    [pad->controller release];
    g_free(pad);
}

static inline uint32_t gc_pressure(float v)
{
    if( v < 0 ) v = -v;
    if( v > 1 ) v = 1;
    return (uint32_t)(v * MAX_PRESSURE);
}

static void gc_install_handlers(gc_pad_driver_t pad)
{
    if( pad->controller.extendedGamepad ) {
        GCExtendedGamepad *gp = pad->controller.extendedGamepad;
        /* Button order: A, B, X, Y, LB, RB, Menu/Options, L3, R3 */
        GCControllerButtonInput *buttons[] = {
            gp.buttonA, gp.buttonB, gp.buttonX, gp.buttonY,
            gp.leftShoulder, gp.rightShoulder,
            gp.buttonMenu ? gp.buttonMenu : gp.buttonOptions,
            gp.leftThumbstickButton, gp.rightThumbstickButton
        };
        guint btnIndex = 0;
        for( guint i=0; i<G_N_ELEMENTS(buttons); i++ ) if( buttons[i] ) {
            btnIndex++;
            __block guint keycode = btnIndex; /* 1-based */
            buttons[i].valueChangedHandler = ^(GCControllerButtonInput *b, float value, BOOL pressed) {
                if( pressed ) {
                    input_event_keydown( (input_driver_t)pad, keycode, MAX_PRESSURE );
                } else {
                    input_event_keyup( (input_driver_t)pad, keycode );
                }
            };
        }
        pad->buttonCount = btnIndex;

        /* Axes: Left X/Y, Right X/Y -> 4 axes -> 8 keycodes */
        typedef struct { GCControllerDirectionPad *stick; } stick_t;
        stick_t sticks[] = { { gp.leftThumbstick }, { gp.rightThumbstick } };
        guint axisNumber = 0;
        for( guint s=0; s<G_N_ELEMENTS(sticks); s++ ) {
            if( !sticks[s].stick ) continue;
            axisNumber++;
            __block guint axisIndexX = axisNumber; /* 1-based */
            sticks[s].stick.xAxis.valueChangedHandler = ^(GCControllerAxisInput *axis, float value) {
                guint posKey = ((axisIndexX - 1) << 1) + pad->buttonCount + 1;
                if( value > 0.1f ) {
                    input_event_keydown( (input_driver_t)pad, posKey, gc_pressure(value) );
                    input_event_keyup( (input_driver_t)pad, posKey+1 );
                } else if( value < -0.1f ) {
                    input_event_keyup( (input_driver_t)pad, posKey );
                    input_event_keydown( (input_driver_t)pad, posKey+1, gc_pressure(value) );
                } else {
                    input_event_keyup( (input_driver_t)pad, posKey );
                    input_event_keyup( (input_driver_t)pad, posKey+1 );
                }
            };
            axisNumber++;
            __block guint axisIndexY = axisNumber;
            sticks[s].stick.yAxis.valueChangedHandler = ^(GCControllerAxisInput *axis, float value) {
                guint posKey = ((axisIndexY - 1) << 1) + pad->buttonCount + 1;
                if( value > 0.1f ) {
                    input_event_keydown( (input_driver_t)pad, posKey, gc_pressure(value) );
                    input_event_keyup( (input_driver_t)pad, posKey+1 );
                } else if( value < -0.1f ) {
                    input_event_keyup( (input_driver_t)pad, posKey );
                    input_event_keydown( (input_driver_t)pad, posKey+1, gc_pressure(value) );
                } else {
                    input_event_keyup( (input_driver_t)pad, posKey );
                    input_event_keyup( (input_driver_t)pad, posKey+1 );
                }
            };
        }
        pad->axisCount = axisNumber;

        /* DPad -> map to additional buttons at end */
        if( gp.dpad ) {
            guint base = ++pad->buttonCount; // up
            gp.dpad.up.valueChangedHandler = ^(GCControllerButtonInput *b, float value, BOOL pressed){
                pressed ? input_event_keydown((input_driver_t)pad, base, MAX_PRESSURE)
                        : input_event_keyup((input_driver_t)pad, base);
            };
            gp.dpad.down.valueChangedHandler = ^(GCControllerButtonInput *b, float value, BOOL pressed){
                pressed ? input_event_keydown((input_driver_t)pad, base+1, MAX_PRESSURE)
                        : input_event_keyup((input_driver_t)pad, base+1);
            };
            gp.dpad.left.valueChangedHandler = ^(GCControllerButtonInput *b, float value, BOOL pressed){
                pressed ? input_event_keydown((input_driver_t)pad, base+2, MAX_PRESSURE)
                        : input_event_keyup((input_driver_t)pad, base+2);
            };
            gp.dpad.right.valueChangedHandler = ^(GCControllerButtonInput *b, float value, BOOL pressed){
                pressed ? input_event_keydown((input_driver_t)pad, base+3, MAX_PRESSURE)
                        : input_event_keyup((input_driver_t)pad, base+3);
            };
            pad->buttonCount += 3; // we already counted +1 for up above
        }
    }
}

static void gc_on_connect(GCController *controller)
{
    gc_pad_driver_t pad = g_malloc0(sizeof(*pad));
    pad->controller = [controller retain];
    pad->base.resolve_keysym = gc_resolve_keysym;
    pad->base.get_keysym_for_keycode = gc_keysym_for_keycode;
    pad->base.destroy = gc_destroy;
    gchar *id = g_strdup_printf("GC%u", gc_next_id++);
    pad->base.id = id; // not freed for process lifetime
    gc_install_handlers(pad);
    guint max_keycode = pad->buttonCount + pad->axisCount*2;
    input_register_device((input_driver_t)pad, (uint16_t)max_keycode);
    gc_drivers = g_list_append(gc_drivers, pad);
}

static void gc_on_disconnect(GCController *controller)
{
    GList *it = gc_drivers;
    while( it ) {
        gc_pad_driver_t pad = (gc_pad_driver_t)it->data;
        if( pad->controller == controller ) {
            input_unregister_device((input_driver_t)pad);
            gc_drivers = g_list_delete_link(gc_drivers, it);
            return;
        }
        it = it->next;
    }
}

void gc_osx_init(void)
{
    [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification
                                                      object:nil queue:[NSOperationQueue mainQueue]
                                                  usingBlock:^(NSNotification *n){ gc_on_connect(n.object); }];
    [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification
                                                      object:nil queue:[NSOperationQueue mainQueue]
                                                  usingBlock:^(NSNotification *n){ gc_on_disconnect(n.object); }];
    [GCController startWirelessControllerDiscoveryWithCompletionHandler:^{}];

    for( GCController *c in GCController.controllers ) {
        gc_on_connect(c);
    }
}


