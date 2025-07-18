#include "profile.h"
// use custom_fonts.h only for the draw_active_profile_text function
#include "../assets/custom_fonts.h"
#include <stdio.h>
#include <zephyr/kernel.h>

LV_IMG_DECLARE(profiles);

// MC: mejor implementación
static void draw_active_profile_text(lv_obj_t *canvas,
                                     const struct status_state *state) {
  // new label_dsc
  lv_draw_label_dsc_t label_dsc;
  init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_8,
                 LV_TEXT_ALIGN_LEFT);

  // buffer size should be enough for largest number + null character
  char text[14] = {};
  #if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  snprintf(text, sizeof(text), "%d", state->active_profile_index + 1);
  #else
  // For peripheral builds, show default profile 1
  snprintf(text, sizeof(text), "1");
  #endif

  lv_canvas_draw_text(canvas, 25, 32, 35, &label_dsc, text);
}

void draw_profile_status(lv_obj_t *canvas, const struct status_state *state) {
  draw_active_profile_text(canvas, state);
  // Removed layer indicator dots
  // draw_inactive_profiles(canvas, state);
  // draw_active_profile(canvas, state);
}
