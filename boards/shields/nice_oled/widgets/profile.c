#include "profile.h"
// use custom_fonts.h only for the draw_active_profile_text function
#include "../assets/custom_fonts.h"
#include <stdio.h>
#include <zephyr/kernel.h>

LV_IMG_DECLARE(profiles);

static void draw_inactive_profiles(lv_obj_t *canvas,
                                   const struct status_state *state, int y_offset) {
  lv_draw_img_dsc_t img_dsc;
  lv_draw_img_dsc_init(&img_dsc);

  // Draw profile bar at y_offset + 10 instead of fixed y=110
  lv_canvas_draw_img(canvas, 0, y_offset + 10, &profiles, &img_dsc);
}

static void draw_active_profile(lv_obj_t *canvas,
                                const struct status_state *state, int y_offset) {
  lv_draw_rect_dsc_t rect_white_dsc;
  init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);
  int offset = state->active_profile_index * 7;
  // Draw active profile indicator at y_offset + 10 instead of fixed y=110
  lv_canvas_draw_rect(canvas, 0 + offset, y_offset + 10, 3, 3, &rect_white_dsc);
}

void draw_profile_status(lv_obj_t *canvas, const struct status_state *state, int y_offset) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono, LV_TEXT_ALIGN_LEFT);

    char text[16] = {};  // Increased buffer size to fix truncation warning
    snprintf(text, sizeof(text), "%d", state->active_profile_index + 1);
    // Draw at y_offset instead of fixed y
    lv_canvas_draw_text(canvas, 25, y_offset, 35, &label_dsc, text);
    draw_inactive_profiles(canvas, state, y_offset);
    draw_active_profile(canvas, state, y_offset);
}
