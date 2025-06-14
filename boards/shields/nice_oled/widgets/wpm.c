#include "wpm.h"
#include "../assets/custom_fonts.h"
#include <math.h>
#include <zephyr/kernel.h>

LV_IMG_DECLARE(gauge);
LV_IMG_DECLARE(grid);

static void draw_gauge(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);
    // Restore original position (y=70)
    lv_canvas_draw_img(canvas, 0, 70, &gauge, &img_dsc);
}

static void draw_needle(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);
    int centerX = 12; // 16 default
    int centerY = 90; // 100 gut, 66 default
    int offset = 5;   // 5 def, largo de la aguja
    int value = state->wpm[9];

#if IS_ENABLED(CONFIG_NICE_OLED_GEM_ANIMATION_WPM_FIXED_RANGE)
    float max = CONFIG_NICE_OLED_GEM_ANIMATION_WPM_FIXED_RANGE_MAX;
#else
    float max = 0;
    for (int i = 0; i < 10; i++) {
        if (state->wpm[i] > max) {
            max = state->wpm[i];
        }
    }
#endif
    if (max == 0)
        max = 100;
    if (value < 0)
        value = 0;
    if (value > max)
        value = max;

    float radius = 25.45585;
    float angleDeg = 225 + ((float)value / max) * 90;
    float angleRad = angleDeg * (3.14159 / 180.0f);

    int needleStartX = centerX + (int)(offset * cos(angleRad));
    int needleStartY = centerY + (int)(offset * sin(angleRad));
    int needleEndX = centerX + (int)(radius * cos(angleRad));
    int needleEndY = centerY + (int)(radius * sin(angleRad));

    lv_point_t points[2] = {{needleStartX, needleStartY}, {needleEndX, needleEndY}};
    lv_canvas_draw_line(canvas, points, 2, &line_dsc);
}

static void draw_grid(lv_obj_t *canvas) {
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);
    // Restore original position (y=95)
    lv_canvas_draw_img(canvas, -1, 95, &grid, &img_dsc);
}

static void draw_graph(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 2);
    lv_point_t points[10];

#if IS_ENABLED(CONFIG_NICE_OLED_GEM_ANIMATION_WPM_FIXED_RANGE)
    int max = CONFIG_NICE_OLED_GEM_ANIMATION_WPM_FIXED_RANGE_MAX;
    if (max == 0) {
        max = 100;
    }

    int value = 0;
    for (int i = 0; i < 10; i++) {
        value = state->wpm[i];
        if (value > max) {
            value = max;
        }
        points[i].x = -36 + i * 7.4;
        points[i].y = 127 - (value * 32 / max);
    }
#else
    int max = 0;
    int min = 256;

    for (int i = 0; i < 10; i++) {
        if (state->wpm[i] > max) {
            max = state->wpm[i];
        }
        if (state->wpm[i] < min) {
            min = state->wpm[i];
        }
    }

    int range = max - min;
    if (range == 0) {
        range = 1;
    }

    for (int i = 0; i < 10; i++) {
        points[i].x = 0 + i * 7.4;
        points[i].y = 97 - (state->wpm[i] - min) * 32 / range;
    }
#endif

    lv_canvas_draw_line(canvas, points, 10, &line_dsc);
}

static void draw_label(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc_wpm;
    init_label_dsc(&label_dsc_wpm, LVGL_FOREGROUND, &pixel_operator_mono_12, LV_TEXT_ALIGN_LEFT);

    char wpm_text[10] = {};

    snprintf(wpm_text, sizeof(wpm_text), "%d", state->wpm[9]);
    // Restore original label positions (y=75)
    if (state->wpm[9] < 10) {
        lv_canvas_draw_text(canvas, 12, 75, 50, &label_dsc_wpm, wpm_text);
    } else if (state->wpm[9] >= 10 && state->wpm[9] < 100) {
        lv_canvas_draw_text(canvas, 9, 75, 50, &label_dsc_wpm, wpm_text);
    } else {
        lv_canvas_draw_text(canvas, 7, 75, 50, &label_dsc_wpm, wpm_text);
    }
}

void draw_wpm_status(lv_obj_t *canvas, const struct status_state *state) {
    draw_gauge(canvas, state);
    draw_needle(canvas, state);
    draw_grid(canvas);
    draw_graph(canvas, state);
    draw_label(canvas, state);
}
