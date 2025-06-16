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

#include "animation.h"
#include "battery.h"
#include "output.h"
#include "screen_peripheral.h"
#include "wpm.h"
#include "layer.h" 
#include "profile.h"
#include "display_split_sync.h"
#include "peripheral_extended_state.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

// Extended state that can hold synced data
static struct peripheral_extended_status_state extended_state = {0};

// Forward declarations
static void on_sync_data_received(const struct display_sync_data *data);
static void update_display(void);

/**
 * Draw canvas
 **/

static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);

    // TEMPORARY: Force sync active for testing - uncomment next line to test meters
    // extended_state.sync_active = true;

    // Peripheral display: Show FULL meters layout using synced data
    if (extended_state.sync_active) {
        // We have synced data - show full meters like original central display!
        
        // No background - clean meters-only layout
        draw_output_status(canvas, state);
        draw_wpm_status(canvas, state);
        draw_battery_status(canvas, state); 
        draw_profile_status(canvas, state);
        draw_layer_status(canvas, state);
        
        // Add small sync indicator
        lv_draw_label_dsc_t label_dsc;
        init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_8, LV_TEXT_ALIGN_RIGHT);
        lv_area_t sync_area = {.x1 = 50, .y1 = 0, .x2 = 64, .y2 = 8};
        lv_draw_label(canvas, &sync_area, &label_dsc, "SYNC", NULL);
        
    } else {
        // Fallback: basic peripheral display if no sync
        draw_background(canvas);
        draw_output_status(canvas, state);
        draw_battery_status(canvas, state);
        
        // Show "NO SYNC" indicator prominently
        lv_draw_label_dsc_t label_dsc;
        init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
        lv_area_t text_area = {.x1 = 0, .y1 = 40, .x2 = 64, .y2 = 60};
        lv_draw_label(canvas, &text_area, &label_dsc, "NO SYNC", NULL);
    }

    // Rotate for horizontal display
    rotate_canvas(canvas, cbuf);
}

/**
 * Split sync callback - called when data is received from central
 **/
static void on_sync_data_received(const struct display_sync_data *data) {
    if (!data) {
        return;
    }
    
    // Update extended state with synced data
    update_extended_state_with_sync(&extended_state, data);
    
    // Trigger display update
    update_display();
}

/**
 * Update display with current extended state
 **/
static void update_display(void) {
    struct status_state *state = extended_to_status_state(&extended_state);
    if (!state) {
        return;
    }
    
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        draw_canvas(widget->obj, widget->cbuf, state);
    }
}

/**
 * Battery status
 **/

static void set_battery_status(struct zmk_widget_screen *widget,
                               struct battery_status_state state) {

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    extended_state.charging = state.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

    extended_state.battery = state.level;

    update_display();
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
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state);

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

/**
 * Peripheral status
 **/

static struct peripheral_status_state get_state(const zmk_event_t *_eh) {
    return (struct peripheral_status_state){.connected = zmk_split_bt_peripheral_is_connected()};
}

static void set_connection_status(struct zmk_widget_screen *widget,
                                  struct peripheral_status_state state) {
    extended_state.connected = state.connected;

    update_display();
}

static void output_status_update_cb(struct peripheral_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_connection_status(widget, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct peripheral_status_state,
                            output_status_update_cb, get_state)
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
    draw_animation(canvas, widget);
    
    // Initialize local status tracking
    widget_battery_status_init();
    widget_peripheral_status_init();

    // Initialize split sync system and register callback
    #if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    display_split_sync_init();
    display_split_sync_register_callback(on_sync_data_received);
    #endif

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { return widget->obj; }
