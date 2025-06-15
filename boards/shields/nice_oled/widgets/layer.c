#include "layer.h"
#include "../assets/custom_fonts.h"
#include <ctype.h> // Para toupper()
#include <zephyr/kernel.h>

// MC: better implementation
void draw_layer_status(lv_obj_t *canvas, const struct status_state *state, int y_offset) {
  lv_draw_label_dsc_t label_dsc;
  init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono,
                 LV_TEXT_ALIGN_LEFT);

  char text[14] = {};
  int result = 0;

  if (state->layer_label == NULL) {
    result = snprintf(text, sizeof(text), "LAYER %i", state->layer_index);
  } else {
    result = snprintf(text, sizeof(text), "%s", state->layer_label);
    for (int i = 0; text[i] != '\0'; i++) {
      text[i] = toupper(text[i]);
    }
  }

  if (result >= sizeof(text)) {
    LV_LOG_WARN("truncated");
  }

  // Draw at y_offset instead of fixed y=80
  lv_canvas_draw_text(canvas, 0, y_offset, 68, &label_dsc, text);
}
