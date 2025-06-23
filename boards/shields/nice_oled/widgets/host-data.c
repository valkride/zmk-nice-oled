#include "host-data.h"
#include "../assets/custom_fonts.h"
#include <zephyr/kernel.h>

void draw_host_data_status(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono, LV_TEXT_ALIGN_LEFT);

    char text[16] = {};
    snprintf(text, sizeof(text), "GPU: 100%%");
    
    // Position the text in the middle of the space between the lines
    // y=65 (top line) to y=140 (bottom line) = 75px height
    // Center it around y=102 (65 + 75/2 = 102.5)
    lv_canvas_draw_text(canvas, 0, 100, 68, &label_dsc, text);
}
