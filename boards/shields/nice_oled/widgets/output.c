#include "output.h"
#include "../assets/custom_fonts.h"
#include <zephyr/kernel.h>

LV_IMG_DECLARE(bt_no_signal);
LV_IMG_DECLARE(bt_unbonded);
LV_IMG_DECLARE(bt);
LV_IMG_DECLARE(usb);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static void draw_usb_connected(lv_obj_t *canvas, int y_offset) {
  lv_draw_img_dsc_t img_dsc;
  lv_draw_img_dsc_init(&img_dsc);

  lv_canvas_draw_img(canvas, 0, 34 + y_offset, &usb, &img_dsc);
}

static void draw_ble_unbonded(lv_obj_t *canvas, int y_offset) {
  lv_draw_img_dsc_t img_dsc;
  lv_draw_img_dsc_init(&img_dsc);

  lv_canvas_draw_img(canvas, -1, 32 + y_offset, &bt_unbonded, &img_dsc);
}
#endif

static void draw_ble_disconnected(lv_obj_t *canvas, int y_offset) {
  lv_draw_img_dsc_t img_dsc;
  lv_draw_img_dsc_init(&img_dsc);

  lv_canvas_draw_img(canvas, 4, 32 + y_offset, &bt_no_signal, &img_dsc);
}

static void draw_ble_connected(lv_obj_t *canvas, int y_offset) {
  lv_draw_img_dsc_t img_dsc;
  lv_draw_img_dsc_init(&img_dsc);

  lv_canvas_draw_img(canvas, 4, 32 + y_offset, &bt, &img_dsc);
}

void draw_output_status(lv_obj_t *canvas, const struct status_state *state, int y_offset) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono, LV_TEXT_ALIGN_LEFT);

    char text[16] = {};
    // ZMK-style: show USB, BLE N, or N/A, no prefix, always fits
    if (state->selected_endpoint.transport == 1) { // 1 = USB
        strcpy(text, "USB");
        #if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        draw_usb_connected(canvas, y_offset);
        #endif
    } else if (state->selected_endpoint.transport == 2) { // 2 = BLE
        snprintf(text, sizeof(text), "BLE %d", state->active_profile_index + 1);
        // Draw bluetooth icons based on connection status
        if (state->active_profile_bonded) {
            if (state->active_profile_connected) {
                draw_ble_connected(canvas, y_offset);
            } else {
                draw_ble_disconnected(canvas, y_offset);
            }
        } else {
            #if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
            draw_ble_unbonded(canvas, y_offset);
            #else
            draw_ble_disconnected(canvas, y_offset);
            #endif
        }
    } else {
        strcpy(text, "N/A");
    }

    // Draw at y_offset instead of fixed y
    lv_canvas_draw_text(canvas, 0, y_offset, 68, &label_dsc, text);
}
