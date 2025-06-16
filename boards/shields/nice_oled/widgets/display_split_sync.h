#pragma once

#include <zephyr/kernel.h>
#include <zmk/endpoints.h>

#define DISPLAY_SYNC_WPM_DATA_SIZE 10

/**
 * Custom display data structure for split sync
 * This contains all the data we want to sync from central to peripheral
 */
struct display_sync_data {
    // WPM data
    uint8_t wpm[DISPLAY_SYNC_WPM_DATA_SIZE];
    
    // Layer data
    uint8_t layer_index;
    char layer_label[16]; // Fixed size string for sync
    
    // Profile data
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    
    // Endpoint data
    struct zmk_endpoint_instance selected_endpoint;
    
    // Sync metadata
    uint32_t sync_timestamp;
};

/**
 * Initialize display split sync system
 */
int display_split_sync_init(void);

/**
 * Send display data from central to peripheral
 */
int display_split_sync_send_data(const struct display_sync_data *data);

/**
 * Callback for when display sync data is received on peripheral
 */
typedef void (*display_sync_received_callback_t)(const struct display_sync_data *data);

/**
 * Register callback for receiving display sync data
 */
void display_split_sync_register_callback(display_sync_received_callback_t callback);

/**
 * Get last received sync data (for peripheral)
 */
const struct display_sync_data* display_split_sync_get_last_data(void);
