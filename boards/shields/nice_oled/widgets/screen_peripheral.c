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
#include "display_split_sync.h"
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
    }
    
    // Calculate WPM (assuming average 5 characters per word)
    wpm_state.current_wpm = (recent_keypresses * 60) / 5;
    
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
    
    // Recalculate WPM
    calculate_wpm();
}

// Callback to receive keypress data from central via split sync
static void central_keypress_received(const struct display_sync_data *sync_data) {
    if (!sync_data) {
        return;
    }
    
    // If sync data contains a keypress timestamp, add it to our tracking
    if (sync_data->sync_timestamp > wpm_state.last_update_time) {
        add_keypress_timestamp(sync_data->sync_timestamp);
        
        uint32_t now = k_uptime_get_32();
        if (now - wpm_state.last_update_time > 500) { // Update display
            update_wpm_display();
            wpm_state.last_update_time = now;
        }
    }
}

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
        if (now - wpm_state.last_update_time > 1000) { // Update at most once per second
            update_wpm_display();
            wpm_state.last_update_time = now;
        }
    }
    
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(wpm_position, wpm_position_listener);
ZMK_SUBSCRIPTION(wpm_position, zmk_position_state_changed);

/**
 * Simple WPM display with static data for peripheral
 **/

static struct k_work_delayable wpm_work;

static void wpm_work_handler(struct k_work *work) {
    // Very lightweight WPM simulation for peripheral display
    static uint8_t counter = 0;
    
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        // Simple animated WPM display - just for visual demonstration
        counter = (counter + 1) % 60;
        
        // Shift array and add new value
        for (int i = 0; i < 9; i++) {
            widget->state.wpm[i] = widget->state.wpm[i + 1];
        }
        
        // Simple pattern for demonstration
        widget->state.wpm[9] = (counter % 20) + 10; // Values between 10-30
        
        update_display();
    }
      // Schedule next update
    k_work_schedule(&wpm_work, K_SECONDS(5));
}

static void init_wpm_timer(void) {
    k_work_init_delayable(&wpm_work, wpm_work_handler);
    k_work_schedule(&wpm_work, K_SECONDS(5)); // Start after 5 seconds
}

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
      // Start WPM timer for peripheral display
    init_wpm_timer();
      // Register for split sync to receive keypress data from central
    display_split_sync_register_callback(central_keypress_received);
    display_split_sync_init();  // Enable split sync system

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { return widget->obj; }
