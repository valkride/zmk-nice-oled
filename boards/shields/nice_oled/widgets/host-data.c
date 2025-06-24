#include "host-data.h"
#include "../assets/custom_fonts.h"
#include <zephyr/kernel.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <raw_hid/events.h>
#include <string.h>
#include <stdlib.h>

// Global variables to store the latest host data
static char g_cpu[4] = "000";
static char g_ram[4] = "000";
static char g_gpu[4] = "000";
static char g_disk[4] = "000";
static char g_date[7] = "000000";
static char g_time[5] = "0000";

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

// Timer for periodic redraws (every 60 seconds)
static struct k_work_delayable redraw_work;
static bool timer_initialized = false;

// Forward declaration
static void redraw_work_handler(struct k_work *work);

// Parse HID data and update global variables
static void parse_hid_data(uint8_t *data, uint8_t length) {
    if (length >= 25) {  // Minimum length needed for all data
        // Correct data format based on your specification:
        // CPU(003) + RAM(030) + GPU(009) + DSK(005) + DATE(230625) + TIME(2038) + VOL(076)
        // '0030300090052306252038076'
        //  0123456789012345678901234        // Shift all positions by +1
        // Data: 0040290100052306252222076
        //       0123456789012345678901234        // No offset - CPU: extract all 3 digits (positions 0,1,2)
        char cpu_str[4];
        cpu_str[0] = data[0];
        cpu_str[1] = data[1]; 
        cpu_str[2] = data[2];
        cpu_str[3] = '\0';
        int cpu_val = (cpu_str[0] - '0') * 100 + (cpu_str[1] - '0') * 10 + (cpu_str[2] - '0');
        snprintf(g_cpu, sizeof(g_cpu), "%d", cpu_val);
          // RAM: extract all 3 digits (positions 3,4,5)
        char ram_str[4];
        ram_str[0] = data[3];
        ram_str[1] = data[4];
        ram_str[2] = data[5];
        ram_str[3] = '\0';
        int ram_val = (ram_str[0] - '0') * 100 + (ram_str[1] - '0') * 10 + (ram_str[2] - '0');
        snprintf(g_ram, sizeof(g_ram), "%d", ram_val);
        
        // GPU: extract all 3 digits (positions 6,7,8)
        char gpu_str[4];
        gpu_str[0] = data[6];
        gpu_str[1] = data[7];
        gpu_str[2] = data[8];
        gpu_str[3] = '\0';
        int gpu_val = (gpu_str[0] - '0') * 100 + (gpu_str[1] - '0') * 10 + (gpu_str[2] - '0');
        snprintf(g_gpu, sizeof(g_gpu), "%d", gpu_val);
        
        // DSK: extract all 3 digits (positions 9,10,11)
        char dsk_str[4];
        dsk_str[0] = data[9];
        dsk_str[1] = data[10];
        dsk_str[2] = data[11];
        dsk_str[3] = '\0';
        int dsk_val = (dsk_str[0] - '0') * 100 + (dsk_str[1] - '0') * 10 + (dsk_str[2] - '0');
        snprintf(g_disk, sizeof(g_disk), "%d", dsk_val);
        
        // Extract DATE (positions 12-17: '230625') → 23/06/25        // Extract DATE (positions 12-17: '230625')
        g_date[0] = data[12];
        g_date[1] = data[13]; 
        g_date[2] = data[14];
        g_date[3] = data[15];
        g_date[4] = data[16];
        g_date[5] = data[17];
        g_date[6] = '\0';        // Extract TIME (positions 18-21: HHMM format)
        g_time[0] = data[18];
        g_time[1] = data[19];
        g_time[2] = data[20];
        g_time[3] = data[21];
        g_time[4] = '\0';
        
        // VOLUME is at positions 22-24: '076' → 76% (we can ignore this for now)
    }
}

void draw_host_data_status(lv_obj_t *canvas, const struct status_state *state) {
    // Simple left alignment - no more centering attempts
    lv_draw_label_dsc_t label_small_dsc;
    init_label_dsc(&label_small_dsc, LVGL_FOREGROUND, &pixel_operator_mono_8, LV_TEXT_ALIGN_LEFT);
    
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_12, LV_TEXT_ALIGN_LEFT);
    
    // CPU
    lv_canvas_draw_text(canvas, 0, 70, 68, &label_small_dsc, "CPU");
    char cpu_text[8] = {};
    snprintf(cpu_text, sizeof(cpu_text), "%s", g_cpu);
    lv_canvas_draw_text(canvas, 0, 78, 68, &label_dsc, cpu_text);
    
    // RAM
    lv_canvas_draw_text(canvas, 0, 88, 68, &label_small_dsc, "RAM");
    char ram_text[8] = {};
    snprintf(ram_text, sizeof(ram_text), "%s", g_ram);
    lv_canvas_draw_text(canvas, 0, 96, 68, &label_dsc, ram_text);
    
    // GPU
    lv_canvas_draw_text(canvas, 0, 106, 68, &label_small_dsc, "GPU");
    char gpu_text[8] = {};
    snprintf(gpu_text, sizeof(gpu_text), "%s", g_gpu);
    lv_canvas_draw_text(canvas, 0, 114, 68, &label_dsc, gpu_text);

    // DSK
    lv_canvas_draw_text(canvas, 0, 124, 68, &label_small_dsc, "DSK");
    char dsk_text[8] = {};
    snprintf(dsk_text, sizeof(dsk_text), "%s", g_disk);
    lv_canvas_draw_text(canvas, 0, 132, 68, &label_dsc, dsk_text);
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
    }    if (event && event->length > 0) {
        const uint8_t *data = event->data;
        
        // Data should be exactly as sent from Python host
        parse_hid_data((uint8_t*)data, event->length);
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
