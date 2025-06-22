#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/usb.h>

#include "battery.h"
#include "output.h"
#include "screen_peripheral.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/**
 * Draw canvas
 **/

static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);
    
    // Simple test - fill with a solid color to see if peripheral display is working
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
    
    // Draw background and status information
    draw_background(canvas);
    draw_output_status(canvas, state);
    draw_battery_status(canvas, state);
}

/**
 * Update procedures
 **/

static void update_screen(struct zmk_widget_screen *widget) {
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

/**
 * Get current state
 **/

static struct status_state get_state(const zmk_event_t *_eh) {
    struct status_state state = {0};
    
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    state.charging = zmk_usb_is_powered();
#else
    state.charging = false;
#endif

    int bat_pct = zmk_battery_state_of_charge();
    if (bat_pct >= 0) {
        state.battery = bat_pct;
    } else {
        state.battery = 0;
    }

#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    state.connected = zmk_split_bt_peripheral_is_connected();
#else
    state.connected = true;
#endif

    return state;
}

/**
 * Battery status widget
 **/

static void set_battery_symbol(struct zmk_widget_screen *widget, struct status_state state) {
    widget->state.battery = state.battery;
    widget->state.charging = state.charging;
    update_screen(widget);
}

static void battery_status_update_cb(struct status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_battery_symbol(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct status_state,
                            battery_status_update_cb, get_state);

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

/**
 * Peripheral connection status
 **/

static void set_connection_status(struct zmk_widget_screen *widget, struct status_state state) {
    widget->state.connected = state.connected;
    update_screen(widget);
}

static void peripheral_status_update_cb(struct status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { 
        set_connection_status(widget, state); 
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct status_state,
                            peripheral_status_update_cb, get_state);
ZMK_SUBSCRIPTION(widget_peripheral_status, zmk_split_peripheral_status_changed);

/**
 * Initialization
 **/

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, CANVAS_HEIGHT, CANVAS_WIDTH);

    lv_obj_t *canvas = lv_canvas_create(widget->obj);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, CANVAS_HEIGHT, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);

    sys_slist_append(&widgets, &widget->node);
    
    // Initialize state
    widget->state = get_state(NULL);
    
    // Initialize status tracking first
    widget_battery_status_init();
    widget_peripheral_status_init();
    
    // Draw initial content after everything is set up
    draw_canvas(widget->obj, widget->cbuf, &widget->state);

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { 
    return widget->obj; 
}
