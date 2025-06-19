#include "display_split_sync.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// WPM sync using a simple shared variable approach for demonstration
// In a real implementation, this would use BLE or UART communication
static uint16_t shared_wpm = 0;
static wpm_sync_received_callback_t wpm_callback = NULL;

// Timer to periodically sync WPM from central to peripheral
static void wpm_sync_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(wpm_sync_work, wpm_sync_work_handler);

static void wpm_sync_work_handler(struct k_work *work) {
    // On peripheral, call the callback with the shared WPM value
    #if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    if (wpm_callback) {
        wpm_callback(shared_wpm);
        LOG_DBG("Peripheral: Received WPM sync value: %d", shared_wpm);
    }
    #endif
    
    // Reschedule for next sync
    k_work_schedule(&wpm_sync_work, K_MSEC(1000)); // Sync every 1 second
}

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
// Central implementation - sends WPM data to peripheral

int display_split_sync_send_wpm(uint16_t wpm) {
    // Store WPM value for peripheral to read
    shared_wpm = wpm;
    LOG_DBG("Central: Sending WPM value %d to peripheral", wpm);
    return 0;
}

int display_split_sync_init(void) {
    LOG_INF("Initializing WPM sync (central)");
    // Start sync timer
    k_work_schedule(&wpm_sync_work, K_MSEC(2000)); // Initial delay
    return 0;
}

#else
// Peripheral implementation - receives WPM data from central

int display_split_sync_send_wpm(uint16_t wpm) {
    // Peripheral doesn't send WPM data
    return -ENOTSUP;
}

int display_split_sync_init(void) {
    LOG_INF("Initializing WPM sync (peripheral)");
    // Start sync timer to receive updates
    k_work_schedule(&wpm_sync_work, K_MSEC(2000)); // Initial delay
    return 0;
}

#endif

void display_split_sync_register_wpm_callback(wpm_sync_received_callback_t callback) {
    wpm_callback = callback;
    LOG_DBG("Registered WPM sync callback");
}
