#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>
#include <zmk/wpm.h>
#include <string.h>

#include "battery.h"
#include "display_split_sync.h"
#include "layer.h"
#include "output.h"
#include "profile.h"
#include "screen.h"
#include "wpm.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

// Note: sync_data_to_peripheral function removed - WPM now handled independently by peripheral

/**
 * luna
 **/

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
#include "luna.h"
static struct zmk_widget_luna luna_widget;
#endif

/**
 * modifiers
 **/
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS)
#include "modifiers.h"                               // Incluir el archivo de cabecera de modifiers
static struct zmk_widget_modifiers modifiers_widget; // Declarar el widget de modifiers
#endif

/**
 * hid indicators
 **/

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_HID_INDICATORS)
#include "hid_indicators.h"
static struct zmk_widget_hid_indicators hid_indicators_widget;
#endif

/**
 * Draw canvas
 **/

static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);    // Central/Main (right) display: Show Bluetooth, battery, layer, and profile
    draw_background(canvas);
    draw_output_status(canvas, state);
    draw_battery_status(canvas, state);
    draw_layer_status(canvas, state);
    draw_profile_status(canvas, state);

    // Note: Data sync to peripheral removed - peripheral now handles WPM independently

    // Rotate for horizontal display
    rotate_canvas(canvas, cbuf);
}

/**
 * Battery status
 **/

static void set_battery_status(struct zmk_widget_screen *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

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
 * Layer status
 **/

static void set_layer_status(struct zmk_widget_screen *widget, struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_status(widget, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){.index = index, .label = zmk_keymap_layer_name(index)};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

/**
 * Output status
 **/

static void set_output_status(struct zmk_widget_screen *widget,
                              const struct output_status_state *state) {
    widget->state.selected_endpoint = state->selected_endpoint;
    widget->state.active_profile_index = state->active_profile_index;
    widget->state.active_profile_connected = state->active_profile_connected;
    widget->state.active_profile_bonded = state->active_profile_bonded;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_output_status(widget, &state); }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    return (struct output_status_state){
        .selected_endpoint = zmk_endpoints_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

/**
 * WPM status - track all keypresses for sync to peripheral
 **/

static struct {
    uint32_t keypress_timestamps[50]; // Store last 50 keypress times
    uint8_t keypress_count;
    uint32_t last_sync_time;
    uint16_t current_wpm;
} central_wpm_state = {0};

static void calculate_central_wpm(void) {
    uint32_t now = k_uptime_get_32();
    uint32_t time_window = 60000; // 1 minute window
    
    // Count keypresses in the last minute
    uint8_t recent_keypresses = 0;
    for (int i = 0; i < central_wpm_state.keypress_count && i < 50; i++) {
        if (now - central_wpm_state.keypress_timestamps[i] <= time_window) {
            recent_keypresses++;
        }
    }
    
    // Calculate WPM: (keypresses per minute) / 5 (avg chars per word)
    if (recent_keypresses > 0) {
        central_wpm_state.current_wpm = recent_keypresses / 5;
        
        // For better responsiveness, also consider a shorter 15-second window
        uint8_t recent_15s = 0;
        uint32_t short_window = 15000; // 15 seconds
        for (int i = 0; i < central_wpm_state.keypress_count && i < 50; i++) {
            if (now - central_wpm_state.keypress_timestamps[i] <= short_window) {
                recent_15s++;
            }
        }
        
        // Calculate 15-second WPM (extrapolated to per minute)
        if (recent_15s > 0) {
            uint16_t short_wpm = (recent_15s * 4) / 5; // 15s * 4 = 60s, then /5 for words
            // Use the higher value for more responsive display
            if (short_wpm > central_wpm_state.current_wpm) {
                central_wpm_state.current_wpm = short_wpm;
            }
        }
        
        // Minimum 1 WPM if there's any recent activity
        if (central_wpm_state.current_wpm == 0 && recent_keypresses > 0) {
            central_wpm_state.current_wpm = 1;
        }
    } else {
        central_wpm_state.current_wpm = 0;
    }
    
    // Cap at reasonable maximum
    if (central_wpm_state.current_wpm > 200) {
        central_wpm_state.current_wpm = 200;
    }
}

static void add_central_keypress_timestamp(uint32_t timestamp) {
    // Add keypress timestamp to our tracking
    if (central_wpm_state.keypress_count < 50) {
        central_wpm_state.keypress_timestamps[central_wpm_state.keypress_count] = timestamp;
        central_wpm_state.keypress_count++;
    } else {
        // Shift array and add new timestamp
        for (int i = 0; i < 49; i++) {
            central_wpm_state.keypress_timestamps[i] = central_wpm_state.keypress_timestamps[i + 1];
        }
        central_wpm_state.keypress_timestamps[49] = timestamp;
    }
    
    // Clean up old timestamps (older than 2 minutes) to keep calculation accurate
    uint32_t now = k_uptime_get_32();
    uint8_t write_index = 0;
    for (uint8_t read_index = 0; read_index < central_wpm_state.keypress_count; read_index++) {
        if (now - central_wpm_state.keypress_timestamps[read_index] <= 120000) { // Keep last 2 minutes
            central_wpm_state.keypress_timestamps[write_index] = central_wpm_state.keypress_timestamps[read_index];
            write_index++;
        }
    }
    central_wpm_state.keypress_count = write_index;
    
    // Recalculate WPM
    calculate_central_wpm();
}

// Listen to ALL position state changes (both central and peripheral keys)
static int central_wpm_position_listener(const zmk_event_t *eh) {
    struct zmk_position_state_changed *pos_ev = as_zmk_position_state_changed(eh);
    
    // Only count key presses (not releases)
    if (pos_ev && pos_ev->state) {
        uint32_t now = k_uptime_get_32();
        add_central_keypress_timestamp(now);
        
        LOG_DBG("Central WPM: Keypress detected, current WPM: %d", central_wpm_state.current_wpm);
        
        // Send keypress notification to peripheral for its WPM calculation
        display_split_sync_send_keypress(now);
    }
    
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(central_wpm_position, central_wpm_position_listener);
ZMK_SUBSCRIPTION(central_wpm_position, zmk_position_state_changed);

static void set_wpm_status_for_sync(struct zmk_widget_screen *widget, struct wpm_status_state state) {
    // Update WPM array for sync to peripheral (not used on central display)
    for (int i = 0; i < 9; i++) {
        widget->state.wpm[i] = widget->state.wpm[i + 1];
    }
    widget->state.wpm[9] = state.wpm;
    
    // Note: WPM data is tracked separately for split sync but not displayed on main screen
}
    // Update WPM array for sync to peripheral
    for (int i = 0; i < 9; i++) {
        widget->state.wpm[i] = widget->state.wpm[i + 1];
    }
    widget->state.wpm[9] = state.wpm;
    
    // Send sync data to peripheral    // struct display_sync_data sync_data = {0}; // REMOVED - no longer using split sync
    // memcpy(sync_data.wpm, widget->state.wpm, sizeof(sync_data.wpm));
    // sync_data.layer_index = widget->state.layer_index;
    // if (widget->state.layer_label) {
    //     strncpy(sync_data.layer_label, widget->state.layer_label, sizeof(sync_data.layer_label) - 1);
    // }
    // sync_data.active_profile_index = widget->state.active_profile_index;
    // sync_data.active_profile_connected = widget->state.active_profile_connected;
    // sync_data.active_profile_bonded = widget->state.active_profile_bonded;
    // sync_data.selected_endpoint = widget->state.selected_endpoint;
    
    // display_split_sync_send_data(&sync_data);
    
    // Note: WPM sync functionality removed - peripheral handles WPM independently
}

/*
static void wpm_status_update_cb_for_sync(struct wpm_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_wpm_status_for_sync(widget, state); }
}

static struct wpm_status_state wpm_status_get_state_for_sync(const zmk_event_t *eh) {
    return (struct wpm_status_state){.wpm = zmk_wpm_get_state()};
}
*/

// WPM status listener removed - functionality moved to peripheral

/**
 * WPM status - REMOVED from main display (now handled by peripheral)
 **/

// WPM functionality moved to peripheral display in screen_peripheral.c
// ZMK's split system automatically forwards all keypresses to the peripheral

/**
 * Initialization
 **/

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, CANVAS_HEIGHT, CANVAS_WIDTH);

    lv_obj_t *canvas = lv_canvas_create(widget->obj);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, CANVAS_HEIGHT, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);    sys_slist_append(&widgets, &widget->node);
    
    // Initialize keypress sync for WPM tracking
    display_split_sync_init();
    
    widget_battery_status_init();
    widget_layer_status_init();
    widget_output_status_init();
    // WPM functionality moved to peripheral - no sync needed

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
    zmk_widget_luna_init(&luna_widget, canvas);
    lv_obj_align(zmk_widget_luna_obj(&luna_widget), LV_ALIGN_TOP_LEFT, 36, 0);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_HID_INDICATORS)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, canvas);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS)
    zmk_widget_modifiers_init(&modifiers_widget, canvas); // Inicializar el widget de modifiers
#endif

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { return widget->obj; }
