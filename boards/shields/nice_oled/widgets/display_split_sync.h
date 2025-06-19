#pragma once

#include <zephyr/kernel.h>

/**
 * Simple keypress sync for WPM calculation
 * Central sends keypress timestamps to peripheral for accurate WPM tracking
 */

/**
 * Initialize keypress sync system
 */
int display_split_sync_init(void);

/**
 * Send keypress timestamp from central to peripheral
 */
int display_split_sync_send_keypress(uint32_t timestamp);

/**
 * Callback for when keypress data is received on peripheral
 */
typedef void (*keypress_sync_received_callback_t)(uint32_t timestamp);

/**
 * Register callback for receiving keypress sync data
 */
void display_split_sync_register_keypress_callback(keypress_sync_received_callback_t callback);

/**
 * Get last received sync data (for peripheral)
 */
const struct display_sync_data* display_split_sync_get_last_data(void);
