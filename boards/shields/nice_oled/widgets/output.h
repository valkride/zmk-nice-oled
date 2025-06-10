#pragma once

#include "util.h"
#include <lvgl.h>
#include <zmk/endpoints.h>

struct output_status_state {
  struct zmk_endpoint_instance selected_endpoint;
  int active_profile_index;
  bool active_profile_connected;
  bool active_profile_bonded;
};

struct peripheral_status_state {
  bool connected;
};

void draw_output_status(lv_obj_t *canvas, const struct status_state *state);
