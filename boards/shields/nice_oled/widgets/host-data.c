#include "host-data.h"
#include "../assets/custom_fonts.h"
#include <zephyr/kernel.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <raw_hid/events.h>
#include <string.h>
#include <stdlib.h>

// Global variables to store the latest host data
static char g_cpu[4] = "045";
static char g_ram[4] = "067";
static char g_gpu[4] = "023";
static char g_disk[4] = "089";
static char g_date[7] = "230625";
static char g_time[5] = "1951";

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

// Timer for periodic redraws (every 60 seconds)
static struct k_work_delayable redraw_work;
static bool timer_initialized = false;

// Forward declaration
static void redraw_work_handler(struct k_work *work);

// Parse HID data and update global variables
static void parse_hid_data(uint8_t *data, uint8_t length) {
    if (length >= 23) {  // Minimum length needed for all data
        // Data format: '0030300090052306252038076'
        // Groups of 3: CPU(003) RAM(030) GPU(009) DSK(005) + DATE/TIME
        
        // Extract CPU (positions 0-2) - convert 3 ASCII digits to number
        int cpu_val = (data[0] - '0') * 100 + (data[1] - '0') * 10 + (data[2] - '0');
        snprintf(g_cpu, sizeof(g_cpu), "%d", cpu_val);
        
        // Extract RAM (positions 3-5) - convert 3 ASCII digits to number
        int ram_val = (data[3] - '0') * 100 + (data[4] - '0') * 10 + (data[5] - '0');
        snprintf(g_ram, sizeof(g_ram), "%d", ram_val);
        
        // Extract GPU (positions 6-8) - convert 3 ASCII digits to number
        int gpu_val = (data[6] - '0') * 100 + (data[7] - '0') * 10 + (data[8] - '0');
        snprintf(g_gpu, sizeof(g_gpu), "%d", gpu_val);
        
        // Extract DSK (positions 9-11) - convert 3 ASCII digits to number
        int dsk_val = (data[9] - '0') * 100 + (data[10] - '0') * 10 + (data[11] - '0');
        snprintf(g_disk, sizeof(g_disk), "%d", dsk_val);
        
        // Extract DATE (positions 12-17) - copy 6 ASCII digits directly
        g_date[0] = data[12];
        g_date[1] = data[13];
        g_date[2] = data[14];
        g_date[3] = data[15];
        g_date[4] = data[16];
        g_date[5] = data[17];
        g_date[6] = '\0';
        
        // Extract TIME (positions 18-21) - copy 4 ASCII digits directly
        g_time[0] = data[18];
        g_time[1] = data[19];
        g_time[2] = data[20];
        g_time[3] = data[21];
        g_time[4] = '\0';
    }
}
        g_date[1] = data[13];
        g_date[2] = data[14];
        g_date[3] = data[15];
        g_date[4] = data[16];
        g_date[5] = data[17];
        g_date[6] = '\0';
        
        // Extract TIME (positions 18-21) - copy 4 ASCII digits directly
        g_time[0] = data[18];
        g_time[1] = data[19];
        g_time[2] = data[20];
        g_time[3] = data[21];
        g_time[4] = '\0';
    }
}

void draw_host_data_status(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_12, LV_TEXT_ALIGN_LEFT);// Format and draw date (DD/MM/YY format from DDMMYY) - left aligned
    char date_text[16] = {};
    snprintf(date_text, sizeof(date_text), "%c%c/%c%c/%c%c", g_date[0], g_date[1], g_date[2], g_date[3], g_date[4], g_date[5]);
    lv_canvas_draw_text(canvas, 0, 68, 50, &label_dsc, date_text);
    
    // Format and draw time (HH:MM format from HHMM) - left aligned
    char time_text[16] = {};
    snprintf(time_text, sizeof(time_text), "%c%c:%c%c", g_time[0], g_time[1], g_time[2], g_time[3]);
    lv_canvas_draw_text(canvas, 0, 78, 50, &label_dsc, time_text);
    
    // Draw system info with full indicator names
    char cpu_text[16] = {};
    snprintf(cpu_text, sizeof(cpu_text), "CPU:%s%%", g_cpu);
    lv_canvas_draw_text(canvas, 0, 88, 50, &label_dsc, cpu_text);
    
    char gpu_text[16] = {};
    snprintf(gpu_text, sizeof(gpu_text), "GPU:%s%%", g_gpu);
    lv_canvas_draw_text(canvas, 0, 98, 50, &label_dsc, gpu_text);

    char ram_text[16] = {};
    snprintf(ram_text, sizeof(ram_text), "RAM:%s%%", g_ram);
    lv_canvas_draw_text(canvas, 0, 108, 50, &label_dsc, ram_text);
    
    char dsk_text[16] = {};
    snprintf(dsk_text, sizeof(dsk_text), "DSK:%s%%", g_disk);
    lv_canvas_draw_text(canvas, 0, 118, 50, &label_dsc, dsk_text);
}

// Update all screen widgets when new HID data is received
static void update_widgets() {
    // We need to trigger a redraw of the main screen widgets
    // The host-data is drawn as part of the main screen's draw_canvas() function
    // So we need to call the screen widget update mechanism
    
    // Since our widget is integrated into the main screen drawing,
    // we don't need to maintain a separate widget list for host-data
    // The data is stored in global variables and will be used
    // the next time the screen redraws for any reason
}

// Work handler for periodic redraws
static void redraw_work_handler(struct k_work *work) {
    // Force a redraw by raising a fake layer state changed event
    // This will trigger the main screen to redraw, which includes our host-data widget
    // We'll raise an event with the current layer state to avoid changing anything
    uint8_t current_layer = 0; // Default layer
    raise_layer_state_changed(current_layer, true);
    
    // Schedule the next redraw in 60 seconds
    k_work_schedule(&redraw_work, K_SECONDS(60));
}

// Initialize the timer system
static void init_redraw_timer() {
    if (!timer_initialized) {
        k_work_init_delayable(&redraw_work, redraw_work_handler);
        k_work_schedule(&redraw_work, K_SECONDS(60));
        timer_initialized = true;
    }
}

// HID event listener
static int host_data_hid_listener(const zmk_event_t *eh) {
    struct raw_hid_received_event *event = as_raw_hid_received_event(eh);
    if (event != NULL) {
        parse_hid_data(event->data, event->length);
        update_widgets();
        
        // Initialize the timer on first HID data received
        init_redraw_timer();
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(host_data_hid_listener, host_data_hid_listener);
ZMK_SUBSCRIPTION(host_data_hid_listener, raw_hid_received_event);

// Widget initialization function
int zmk_widget_host_data_init(struct zmk_widget_host_data *widget, lv_obj_t *parent) {
    widget->obj = parent;
    sys_slist_append(&widgets, &widget->node);
    
    // Initialize the timer for periodic redraws
    init_redraw_timer();
    
    return 0;
}

lv_obj_t *zmk_widget_host_data_obj(struct zmk_widget_host_data *widget) {
    return widget->obj;
}
