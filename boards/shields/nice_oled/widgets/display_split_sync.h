#pragma once

#include <zephyr/kernel.h>

/**
 * WPM sync for split keyboard
 * Central calculates WPM for all keys and sends the value to peripheral
 */

/**
 * Initialize WPM sync system
 */
int display_split_sync_init(void);

/**
 * Send current WPM value from central to peripheral
 */
int display_split_sync_send_wpm(uint16_t wpm);

/**
 * Callback for when WPM data is received on peripheral
 */
typedef void (*wpm_sync_received_callback_t)(uint16_t wpm);

/**
 * Register callback for receiving WPM sync data
 */
void display_split_sync_register_wpm_callback(wpm_sync_received_callback_t callback);
