#pragma once

#include "util.h"
#include <lvgl.h>

struct zmk_widget_host_data {
    sys_snode_t node;
    lv_obj_t *obj;
};

void draw_host_data_status(lv_obj_t *canvas, const struct status_state *state);
int zmk_widget_host_data_init(struct zmk_widget_host_data *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_host_data_obj(struct zmk_widget_host_data *widget);
