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

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static void update_display(void);

/**
 * Status state structures
 **/

/**
 * Draw canvas
 **/

static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);    // Peripheral (left) display: Simple status display
    draw_background(canvas);
    draw_output_status(canvas, state);
    draw_battery_status(canvas, state);
}

/**
 * Animation timer - No longer needed
 **/

// Animation code for peripheral display

/**
 * Update procedures
 **/

static void update_screen(struct zmk_widget_screen *widget) {
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

/**
 * Battery status widget
 **/

static struct status_state get_state(const zmk_event_t *_eh) {
    struct status_state state = {0};
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    state.usb_present = zmk_usb_is_powered();
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

    state.bt_profile_connected = zmk_ble_active_profile_is_connected();
    state.bt_profile_active = zmk_ble_active_profile_index();
    state.layer_active = zmk_keymap_highest_layer_active();

#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    state.connected = zmk_split_bt_peripheral_is_connected();
#else
    state.connected = state.bt_profile_connected;
#endif

    int bat_pct = zmk_battery_state_of_charge();
    if (bat_pct >= 0) {
        state.bat_pct = bat_pct;
    } else {
        state.bat_pct = 0;
    }

    return state;
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.bat_pct = state.level;
        update_screen(widget);
    }
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.usb_present = state.usb_present;
        widget->state.bt_profile_active = state.bt_profile_active;
        widget->state.bt_profile_connected = state.bt_profile_connected;
        update_screen(widget);
    }
}

static void peripheral_status_update_cb(bool connected) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.connected = connected;
        update_screen(widget);
    }
}

// Bluetooth connectivity listener
static int peripheral_bluetooth_listener(const zmk_event_t *eh) {
    struct zmk_split_peripheral_status_changed *ev = as_zmk_split_peripheral_status_changed(eh);
    if (ev) {
        peripheral_status_update_cb(ev->connected);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(peripheral_bluetooth, peripheral_bluetooth_listener);
ZMK_SUBSCRIPTION(peripheral_bluetooth, zmk_split_peripheral_status_changed);

/**
 * WPM tracking system for peripheral display
 **/

// WPM state structure for peripheral tracking
static struct {
    uint32_t keypress_timestamps[50];
    uint8_t keypress_count;
    uint16_t current_wpm;
    uint32_t last_update_time;
    uint8_t wpm_buffer[10];
} wpm_state = {0};

// WPM calculation function
static void calculate_wpm(void) {
    uint32_t now = k_uptime_get_32();
    uint32_t time_window = 60000; // 1 minute
    
    // Count keypresses in the last minute
    uint8_t recent_keypresses = 0;
    for (int i = 0; i < wpm_state.keypress_count && i < 50; i++) {
        if (now - wpm_state.keypress_timestamps[i] <= time_window) {
            recent_keypresses++;
        }
    }
    
    LOG_DBG("WPM Calc: Total keypresses in buffer: %d, Recent keypresses: %d", 
            wpm_state.keypress_count, recent_keypresses);
    
    // Calculate base WPM (words per minute, assuming 5 chars per word)
    uint16_t base_wpm = (recent_keypresses * 60) / 5; // keypresses per minute / 5
    
    if (recent_keypresses > 0) {
        // For more responsiveness, also calculate short-term WPM
        uint32_t short_window = 10000; // 10 seconds
        uint8_t short_keypresses = 0;
        for (int i = 0; i < wpm_state.keypress_count && i < 50; i++) {
            if (now - wpm_state.keypress_timestamps[i] <= short_window) {
                short_keypresses++;
            }
        }
        
        uint16_t short_wpm = (short_keypresses * 60) / 5; // Scale to per minute
        if (short_keypresses >= 3) { // Only use short window if we have enough data
            wpm_state.current_wpm = (short_wpm > base_wpm) ? short_wpm : base_wpm;
        } else {
            wpm_state.current_wpm = base_wpm;
        }
        
        // Minimum WPM if we have recent activity
        if (wpm_state.current_wpm == 0 && recent_keypresses > 0) {
            wpm_state.current_wpm = 1;
        }
    } else {
        // No recent keypresses
        wpm_state.current_wpm = 0;
    }
    
    // Cap at reasonable maximum    if (wpm_state.current_wpm > 200) {
        wpm_state.current_wpm = 200;
    }
    
    LOG_DBG("WPM Calculated: %d (recent: %d, short: %d)", 
            wpm_state.current_wpm, recent_keypresses, 0);
}

// Update WPM display
static void update_wpm_display(void) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        // Update current WPM value
        widget->state.wpm[9] = wpm_state.current_wpm;
        update_screen(widget);
    }
}

// Add keypress timestamp
static void add_keypress_timestamp(uint32_t timestamp) {
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
}

// Work handler for periodic WPM updates
static struct k_work_delayable wpm_update_work;

static void wpm_update_handler(struct k_work *work) {
    // Recalculate WPM periodically
    calculate_wpm();
    
    // Update display
    update_wpm_display();
    
    LOG_DBG("Periodic WPM update: %d", wpm_state.current_wpm);
    
    // Update WPM buffer for graph every few updates
    static int update_counter = 0;
    if (update_counter++ % 5 == 0) {
        // Shift WPM buffer
        for (int i = 9; i > 0; i--) {
            wpm_state.wpm_buffer[i] = wpm_state.wpm_buffer[i-1];
        }
        wpm_state.wpm_buffer[0] = wpm_state.current_wpm;
        
        // Copy to widget state for display
        struct zmk_widget_screen *widget;
        SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
            for (int i = 0; i < 10; i++) {
                widget->state.wpm[i] = wpm_state.wpm_buffer[i];
            }
        }
    }
    
    // Reschedule for next update
    k_work_schedule(&wpm_update_work, K_MSEC(1000)); // Update every second
}

// Position state changed event listener - tracks keypresses for WPM
static int wpm_position_listener(const zmk_event_t *eh) {
    struct zmk_position_state_changed *pos_ev = as_zmk_position_state_changed(eh);
    
    // Only count key presses (not releases)
    if (pos_ev && pos_ev->state) {
        uint32_t now = k_uptime_get_32();
        
        // Debouncing: ignore keypresses that are too close together (less than 50ms apart)
        static uint32_t last_keypress_time = 0;
        if (now - last_keypress_time < 50) {
            LOG_DBG("Peripheral: Debounced keypress (too soon)");
            return ZMK_EV_EVENT_BUBBLE; // Ignore this keypress
        }
        last_keypress_time = now;
        
        LOG_DBG("Peripheral: Valid keypress detected at position %d", pos_ev->position);
        
        // Add keypress timestamp
        add_keypress_timestamp(now);
        
        // Recalculate WPM immediately
        calculate_wpm();
        
        LOG_DBG("Peripheral: After keypress - Count: %d, Current WPM: %d", 
                wpm_state.keypress_count, wpm_state.current_wpm);
        
        // Force immediate display update after keypress
        update_wpm_display();
        
        // Update WPM buffer for graph
        static int buffer_update_counter = 0;
        if (buffer_update_counter++ % 3 == 0) { // Update buffer every 3 keypresses
            for (int i = 9; i > 0; i--) {                wpm_state.wpm_buffer[i] = wpm_state.wpm_buffer[i-1];
            }
            wpm_state.wpm_buffer[0] = wpm_state.current_wpm;
        }
        
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
 * Initialization
 **/

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, CANVAS_HEIGHT, CANVAS_WIDTH);

    lv_obj_t *canvas = lv_canvas_create(widget->obj);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, CANVAS_HEIGHT, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);

    widget->state = get_state(NULL);
    // draw_animation(canvas, widget);

    // Initialize local status tracking
    widget_battery_status_init();
    widget_peripheral_status_init();

    // Initialize WPM data to zero
    for (int i = 0; i < 10; i++) {
        widget->state.wpm[i] = 0;
    }

    // Initialize keypress sync and register callback to receive WPM data from central
    display_split_sync_init();
    
    // Initialize WPM update work    k_work_init_delayable(&wpm_update_work, wpm_update_handler);
    
    // Start periodic WPM updates
    k_work_schedule(&wpm_update_work, K_MSEC(2000)); // Start after 2 seconds
    
    LOG_DBG("Peripheral WPM system initialized");

    sys_slist_append(&widgets, &widget->node);
    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { return widget->obj; }
