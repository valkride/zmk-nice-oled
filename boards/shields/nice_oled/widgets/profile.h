#pragma once

#include <lvgl.h>
#include "util.h"

struct profile_status_state {
    int profile;
};

void draw_profile_status(lv_obj_t *canvas, const struct status_state *state);