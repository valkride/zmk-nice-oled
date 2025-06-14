#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>

#include "animation.h"
#include "battery.h"
#include "layer.h"
#include "output.h"
#include "screen.h"
#include "../assets/custom_fonts.h"

// Fallback macros for event helpers
#ifndef as_zmk_layer_state_changed
#define as_zmk_layer_state_changed(eh) ((const struct zmk_layer_state_changed *)((eh) ? (eh) : NULL))
#endif
#ifndef zmk_event_zmk_layer_state_changed
#define zmk_event_zmk_layer_state_changed (*(const struct zmk_event_type *)0)
#endif
// If ZMK_SUBSCRIPTION is not defined, stub it as a no-op
#ifndef ZMK_SUBSCRIPTION
#define ZMK_SUBSCRIPTION(...)
#endif

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/**
 * Draw activity bar (bluetooth icon + battery percentage)
 **/
static void draw_activity_bar(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono, LV_TEXT_ALIGN_LEFT);
    
    // Create single line with bluetooth symbol and battery percentage
    char display_text[20] = {};
    
    // Always show some text for testing
    if (state == NULL) {
        sprintf(display_text, "TEST 50%%");
    } else {
        #if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        // Only show connection status on central/main keyboard
        if (state->selected_endpoint.transport == 1) { // USB
            sprintf(display_text, "\xEF\x87\xAB " LV_SYMBOL_BATTERY_FULL "%d%%", state->battery); // USB icon + battery
        } else if (state->selected_endpoint.transport == 2) { // BLE
            if (state->active_profile_connected) {
                sprintf(display_text, "\xEF\x8A\x94 " LV_SYMBOL_BATTERY_FULL "%d%%", state->battery); // Bluetooth icon + battery
            } else {
                sprintf(display_text, "\xEF\x87\xB6 " LV_SYMBOL_BATTERY_FULL "%d%%", state->battery); // Disconnected icon + battery
            }
        } else {
            sprintf(display_text, "? " LV_SYMBOL_BATTERY_FULL "%d%%", state->battery);
        }
        #else
        // For peripheral, show PER
        sprintf(display_text, "P " LV_SYMBOL_BATTERY_FULL "%d%%", state->battery);
        #endif
    }
    
    // Draw the complete status line at top-left
    lv_canvas_draw_text(canvas, 0, 0, CANVAS_WIDTH, &label_dsc, display_text);
}

/**
 * Draw layer squares (row of squares indicating layers)
 **/
static void draw_layer_squares(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_rect_dsc_t rect_outline_dsc;
    init_rect_dsc(&rect_outline_dsc, LVGL_FOREGROUND);
    lv_draw_rect_dsc_t rect_filled_dsc;
    init_rect_dsc(&rect_filled_dsc, LVGL_FOREGROUND);
    lv_draw_rect_dsc_t rect_empty_dsc;
    init_rect_dsc(&rect_empty_dsc, LVGL_BACKGROUND);
    
    // Draw 6 layer squares
    for (int i = 0; i < 6; i++) {
        int x = i * 10; // 10 pixels apart
        int y = 15;     // Below the activity bar
        
        // Draw outline for all squares
        lv_canvas_draw_rect(canvas, x, y, 8, 8, &rect_outline_dsc);
        
        // Fill the active layer square
        if (i == state->layer_index) {
            lv_canvas_draw_rect(canvas, x + 1, y + 1, 6, 6, &rect_filled_dsc);
        } else {
            lv_canvas_draw_rect(canvas, x + 1, y + 1, 6, 6, &rect_empty_dsc);
        }
    }
}

/**
 * Draw layer letters (AAAAAA as filler)
 **/
static void draw_layer_letters(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono, LV_TEXT_ALIGN_LEFT);
      // Draw "AAAAAA" as filler text below the squares
    lv_canvas_draw_text(canvas, 0, 28, CANVAS_WIDTH, &label_dsc, "AAAAAA");
}

/**
 * Draw canvas
 **/
static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
    
    // Simple test: just draw "MAIN" text
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono, LV_TEXT_ALIGN_LEFT);
    lv_canvas_draw_text(canvas, 0, 0, CANVAS_WIDTH, &label_dsc, "MAIN");
    lv_canvas_draw_text(canvas, 0, 15, CANVAS_WIDTH, &label_dsc, "SCREEN");
    
    // Original complex drawing (commented out for testing)
    // draw_activity_bar(canvas, state);
    // draw_layer_squares(canvas, state);
    // draw_layer_letters(canvas, state);
    
    rotate_canvas(canvas, cbuf);
}

/**
 * Battery status
 **/
static void set_battery_status(struct zmk_widget_screen *widget, struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif
    widget->state.battery = state.level;
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}
static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state, battery_status_update_cb, battery_status_get_state);
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

/**
 * Output status (for bluetooth/USB icon) - Only on central/main
 **/
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static void set_output_status(struct zmk_widget_screen *widget, struct output_status_state state) {
    widget->state.selected_endpoint = state.selected_endpoint;
    widget->state.active_profile_index = state.active_profile_index;
    widget->state.active_profile_connected = state.active_profile_connected;
    widget->state.active_profile_bonded = state.active_profile_bonded;
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void main_output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_output_status(widget, state); }
}

static struct output_status_state main_output_status_get_state(const zmk_event_t *eh) {
    return (struct output_status_state){
        .selected_endpoint = zmk_endpoints_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_main_output_status, struct output_status_state, main_output_status_update_cb, main_output_status_get_state)
ZMK_SUBSCRIPTION(widget_main_output_status, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_main_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_main_output_status, zmk_ble_active_profile_changed);
#endif
#endif // Output status only on central/main

/**
 * Layer status - simplified for main screen
 **/
static void set_layer_status(struct zmk_widget_screen *widget, uint8_t layer_index) {
    widget->state.layer_index = layer_index;
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { 
        set_layer_status(widget, state.index); 
    }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    // Use the same pattern as screen_peripheral.c
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
    return (struct layer_status_state){
        .index = (ev != NULL) ? ev->layer : 0, 
        .label = "Base"
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_main_layer_status, struct layer_status_state, layer_status_update_cb, layer_status_get_state)
ZMK_SUBSCRIPTION(widget_main_layer_status, zmk_layer_state_changed);

/**
 * Peripheral status
 **/
static struct peripheral_status_state get_state(const zmk_event_t *_eh) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    return (struct peripheral_status_state){.connected = zmk_split_bt_peripheral_is_connected()};
#else
    return (struct peripheral_status_state){};
#endif
}
static void set_connection_status(struct zmk_widget_screen *widget, struct peripheral_status_state state) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    widget->state.connected = state.connected;
#endif
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
static void output_status_update_cb(struct peripheral_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_connection_status(widget, state); }
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct peripheral_status_state, output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_peripheral_status, zmk_split_peripheral_status_changed);

/**
 * Initialization
 **/
int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    LOG_INF("zmk_widget_screen_init called - initializing main screen");
    
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, CANVAS_WIDTH, CANVAS_HEIGHT);
    lv_obj_t *canvas = lv_canvas_create(widget->obj);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    
    LOG_INF("Main screen canvas created, size: %dx%d", CANVAS_WIDTH, CANVAS_HEIGHT);
    
    sys_slist_append(&widgets, &widget->node);
#if defined(draw_animation)
    draw_animation(canvas, widget);
#endif
    
    widget_battery_status_init();
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    widget_main_output_status_init();
#endif    widget_main_layer_status_init();
    widget_peripheral_status_init();
    
    // Force initial drawing with default values
    widget->state.battery = 50;  // Default battery level
    widget->state.layer_index = 0;  // Default layer
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
    
    LOG_INF("Main screen initialization completed");
    return 0;
}
lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { return widget->obj; }
