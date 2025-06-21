#include "peripheral_extended_state.h"
#include <string.h>

static struct status_state converted_state = {0};

struct status_state* extended_to_status_state(struct peripheral_extended_status_state* ext_state) {
    if (!ext_state) {
        return NULL;
    }
    
    // Copy basic peripheral data that exists in both central and peripheral status_state
    converted_state.battery = ext_state->battery;
    converted_state.charging = ext_state->charging;
      // For peripheral builds, the status_state only has battery, charging, and connected fields
    #if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    converted_state.connected = ext_state->connected;
    #else
    // For central builds, copy synced data if available
    if (ext_state->sync_active) {
        converted_state.layer_index = ext_state->layer_index;
        converted_state.layer_label = ext_state->layer_label;
        converted_state.active_profile_index = ext_state->active_profile_index;
        converted_state.active_profile_connected = ext_state->active_profile_connected;
        converted_state.active_profile_bonded = ext_state->active_profile_bonded;
        converted_state.selected_endpoint = ext_state->selected_endpoint;    }
    #endif
    
    return &converted_state;
}
