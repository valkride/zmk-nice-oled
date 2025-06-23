#include "host-data.h"
#include "../assets/custom_fonts.h"
#include <zephyr/kernel.h>

void draw_host_data_status(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_8, LV_TEXT_ALIGN_LEFT);

    // Draw date - try centering with smaller X offset
    char date_text[16] = {};
    snprintf(date_text, sizeof(date_text), "28/12");
    lv_canvas_draw_text(canvas, 15, 68, 50, &label_dsc, date_text);
    
    // Draw time - try centering with smaller X offset  
    char time_text[16] = {};
    snprintf(time_text, sizeof(time_text), "14:30");
    lv_canvas_draw_text(canvas, 15, 78, 50, &label_dsc, time_text);
    
    // Draw system info with shorter text to fit
    char cpu_text[16] = {};
    snprintf(cpu_text, sizeof(cpu_text), "CPU:100");
    lv_canvas_draw_text(canvas, 0, 88, 68, &label_left_dsc, cpu_text);
    
    char gpu_text[16] = {};
    snprintf(gpu_text, sizeof(gpu_text), "GPU:100");
    lv_canvas_draw_text(canvas, 0, 98, 68, &label_left_dsc, gpu_text);
    
    char ram_text[16] = {};
    snprintf(ram_text, sizeof(ram_text), "RAM:100");
    lv_canvas_draw_text(canvas, 0, 108, 68, &label_left_dsc, ram_text);
    
    char dsk_text[16] = {};
    snprintf(dsk_text, sizeof(dsk_text), "DSK:100");
    lv_canvas_draw_text(canvas, 0, 118, 68, &label_left_dsc, dsk_text);
    */
}
