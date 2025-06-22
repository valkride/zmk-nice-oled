#pragma once

#include "util.h"

/**
 * Extended status state for peripheral that includes synced data
 */
struct peripheral_extended_status_state {
    // Original peripheral data
    uint8_t battery;
    bool charging;
    bool connected;
    
    // Synced data from central
    uint8_t layer_index;
    char layer_label[16];
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    struct zmk_endpoint_instance selected_endpoint;
    
    // Sync status
    bool sync_active;
    uint32_t last_sync_time;
};

/**
 * Convert extended state to regular status_state for drawing functions
 */
struct status_state* extended_to_status_state(struct peripheral_extended_status_state* ext_state);
