#pragma once
#ifndef SCREEN_H_
#define SCREEN_H_

#include "util.h"
#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_screen {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_color_t cbuf[CANVAS_WIDTH * CANVAS_HEIGHT];
    struct status_state state;
};

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget);

#endif
