#include "host-data.h"
#include "../assets/custom_fonts.h"
#include <zephyr/kernel.h>

void draw_host_data_status(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_8, LV_TEXT_ALIGN_LEFT);    char text[16] = {};
    snprintf(text, sizeof(text), "GPU:100%%");
    
    // Position the text higher, closer to the top separator line
    lv_canvas_draw_text(canvas, 0, 72, 68, &label_dsc, text);
}
