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
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>

#include "animation.h"
#include "battery.h"
#include "output.h"
#include "screen.h"
#include "../assets/custom_fonts.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/**
 * Draw activity bar (bluetooth icon + battery percentage)
 **/
static void draw_activity_bar(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono, LV_TEXT_ALIGN_LEFT);    // Create single line with bluetooth symbol and battery percentage
    char display_text[20] = {};
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
    
    // Draw the complete status line at top-left
    lv_canvas_draw_text(canvas, 0, 0, 128, &label_dsc, display_text);
}

/**
 * Draw canvas
 **/
static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
    // Main OLED: show only activity bar with bluetooth icon and battery percentage
    draw_activity_bar(canvas, state);
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
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, CANVAS_HEIGHT, CANVAS_WIDTH);
    lv_obj_t *canvas = lv_canvas_create(widget->obj);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, CANVAS_HEIGHT, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);    sys_slist_append(&widgets, &widget->node);
#if defined(draw_animation)
    draw_animation(canvas, widget);
#endif
    widget_battery_status_init();
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    widget_main_output_status_init();
#endif
    widget_peripheral_status_init();
    return 0;
}
lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { return widget->obj; }
