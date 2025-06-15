#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include "util.h"

struct zmk_widget_screen_peripheral {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_color_t cbuf[CANVAS_WIDTH * CANVAS_HEIGHT];
    struct status_state state;
};

int zmk_widget_screen_peripheral_init(struct zmk_widget_screen_peripheral *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_screen_peripheral_obj(struct zmk_widget_screen_peripheral *widget);
