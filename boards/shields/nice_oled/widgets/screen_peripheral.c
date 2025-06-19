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
#include <zmk/events/position_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/usb.h>
#include <string.h>

#include "animation.h"
#include "battery.h"
#include "output.h"
#include "screen_peripheral.h"
#include "wpm.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static void update_display(void);

/**
 * Status state structures
 **/

/**
 * Draw canvas
 **/

static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);    // Peripheral (left) display: Show WPM meter as requested
    draw_background(canvas);
    draw_output_status(canvas, state);
    draw_battery_status(canvas, state);
    draw_wpm_status(canvas, state);  // Re-enabled WPM display
    
    // Rotate for horizontal display
    rotate_canvas(canvas, cbuf);
}

/**
 * Update display with current state
 **/
static void update_display(void) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
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
    widget->state.connected = state.connected;

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
 * Real WPM tracking based on actual key presses
 **/

static struct {
    uint32_t keypress_timestamps[50]; // Store last 50 keypress times
    uint8_t keypress_count;
    uint32_t last_update_time;
    uint16_t current_wpm;
} wpm_state = {0};

static void calculate_wpm(void) {
    uint32_t now = k_uptime_get_32();
    uint32_t time_window = 60000; // 1 minute window
    
    // Count keypresses in the last minute
    uint8_t recent_keypresses = 0;
    for (int i = 0; i < wpm_state.keypress_count && i < 50; i++) {
        if (now - wpm_state.keypress_timestamps[i] <= time_window) {
            recent_keypresses++;
        }
    }    // More responsive WPM calculation
    // Calculate based on actual typing rate
    if (recent_keypresses > 0) {
        // Improved WPM calculation: (keypresses per minute) / 5 (avg chars per word)
        // Since we're looking at a 1-minute window, the formula is simplified
        wpm_state.current_wpm = recent_keypresses / 5;
        
        // For better responsiveness, also consider a shorter 15-second window
        uint8_t recent_15s = 0;
        uint32_t short_window = 15000; // 15 seconds
        for (int i = 0; i < wpm_state.keypress_count && i < 50; i++) {
            if (now - wpm_state.keypress_timestamps[i] <= short_window) {
                recent_15s++;
            }
        }
        
        // Calculate 15-second WPM (extrapolated to per minute)
        if (recent_15s > 0) {
            uint16_t short_wpm = (recent_15s * 4) / 5; // 15s * 4 = 60s, then /5 for words
            // Use the higher value for more responsive display
            if (short_wpm > wpm_state.current_wpm) {
                wpm_state.current_wpm = short_wpm;
            }
        }
        
        // Minimum 1 WPM if there's any recent activity
        if (wpm_state.current_wpm == 0 && recent_keypresses > 0) {
            wpm_state.current_wpm = 1;
        }
    } else {
        wpm_state.current_wpm = 0;
    }
    
    // Cap at reasonable maximum
    if (wpm_state.current_wpm > 200) {
        wpm_state.current_wpm = 200;
    }
}

static void update_wpm_display(void) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        // Shift WPM history array
        for (int i = 0; i < 9; i++) {
            widget->state.wpm[i] = widget->state.wpm[i + 1];
        }
        
        // Add current WPM to the end
        widget->state.wpm[9] = wpm_state.current_wpm;
        
        update_display();
    }
}

static void add_keypress_timestamp(uint32_t timestamp) {
    // Add keypress timestamp to our tracking
    if (wpm_state.keypress_count < 50) {
        wpm_state.keypress_timestamps[wpm_state.keypress_count] = timestamp;
        wpm_state.keypress_count++;
    } else {
        // Shift array and add new timestamp
        for (int i = 0; i < 49; i++) {
            wpm_state.keypress_timestamps[i] = wpm_state.keypress_timestamps[i + 1];
        }
        wpm_state.keypress_timestamps[49] = timestamp;
    }
    
    // Clean up old timestamps (older than 2 minutes) to keep calculation accurate
    uint32_t now = k_uptime_get_32();
    uint8_t write_index = 0;
    for (uint8_t read_index = 0; read_index < wpm_state.keypress_count; read_index++) {
        if (now - wpm_state.keypress_timestamps[read_index] <= 120000) { // Keep last 2 minutes
            wpm_state.keypress_timestamps[write_index] = wpm_state.keypress_timestamps[read_index];
            write_index++;
        }
    }
    wpm_state.keypress_count = write_index;
    
    // Recalculate WPM
    calculate_wpm();
}

// Callback to receive keypress data from central via split sync - REMOVED
// ZMK's split system already provides all keypress events to the peripheral
// via the standard zmk_position_state_changed event system

static int wpm_position_listener(const zmk_event_t *eh) {
    struct zmk_position_state_changed *pos_ev = as_zmk_position_state_changed(eh);
    
    // Only count key presses (not releases)
    if (pos_ev && pos_ev->state) {
        uint32_t now = k_uptime_get_32();
        
        // Add keypress timestamp
        if (wpm_state.keypress_count < 50) {
            wpm_state.keypress_timestamps[wpm_state.keypress_count] = now;
            wpm_state.keypress_count++;
        } else {
            // Shift array and add new timestamp
            for (int i = 0; i < 49; i++) {
                wpm_state.keypress_timestamps[i] = wpm_state.keypress_timestamps[i + 1];
            }
            wpm_state.keypress_timestamps[49] = now;
        }
        
        // Recalculate WPM
        calculate_wpm();
          // Update display if enough time has passed
        if (now - wpm_state.last_update_time > 500) { // Update every 500ms for better responsiveness
            update_wpm_display();
            wpm_state.last_update_time = now;
        }
    }
    
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(wpm_position, wpm_position_listener);
ZMK_SUBSCRIPTION(wpm_position, zmk_position_state_changed);

/**
 * Real-time WPM display - simulation timer removed
 **/

// Old simulation timer removed - WPM now entirely based on real keypresses

/**
 * Initialization
 **/

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, CANVAS_HEIGHT, CANVAS_WIDTH);    lv_obj_t *canvas = lv_canvas_create(widget->obj);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, CANVAS_HEIGHT, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    
    sys_slist_append(&widgets, &widget->node);
    // Boot animation removed for peripheral display to show WPM clearly
    // draw_animation(canvas, widget);    // Initialize local status tracking
    widget_battery_status_init();
    widget_peripheral_status_init();
      // Initialize WPM data to zero
    for (int i = 0; i < 10; i++) {
        widget->state.wpm[i] = 0;
    }
      // Real WPM tracking now handles all updates - no simulation timer needed
    
    // ZMK's split system already forwards all keypresses to peripheral
    // No additional sync system needed

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { return widget->obj; }
