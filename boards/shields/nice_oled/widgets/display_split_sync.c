#include "display_split_sync.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Simple keypress sync using a message queue for inter-thread communication
K_MSGQ_DEFINE(keypress_msgq, sizeof(uint32_t), 10, 4);

static keypress_sync_received_callback_t keypress_callback = NULL;

// Work handler for processing keypress messages
static void keypress_work_handler(struct k_work *work);
K_WORK_DEFINE(keypress_work, keypress_work_handler);

static void keypress_work_handler(struct k_work *work) {
    uint32_t timestamp;
    while (k_msgq_get(&keypress_msgq, &timestamp, K_NO_WAIT) == 0) {
        if (keypress_callback) {
            keypress_callback(timestamp);
        }
    }
}

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
// Central implementation - sends keypress data to peripheral

int display_split_sync_send_keypress(uint32_t timestamp) {
    // In a real BLE implementation, this would send via custom BLE characteristic
    // For now, we'll simulate by directly calling peripheral callback if available
    
    LOG_DBG("Central: Sending keypress timestamp %u", timestamp);
    
    // Put message in queue to be processed by work handler
    int ret = k_msgq_put(&keypress_msgq, &timestamp, K_NO_WAIT);
    if (ret == 0) {
        k_work_submit(&keypress_work);
    }
    
    return ret;
}

int display_split_sync_init(void) {
    LOG_INF("Initializing keypress sync (central)");
    return 0;
}

#else
// Peripheral implementation - receives keypress data from central

int display_split_sync_send_keypress(uint32_t timestamp) {
    // Peripheral doesn't send keypress data
    return -ENOTSUP;
}

int display_split_sync_init(void) {
    LOG_INF("Initializing keypress sync (peripheral)");
    return 0;
}

#endif

void display_split_sync_register_keypress_callback(keypress_sync_received_callback_t callback) {
    keypress_callback = callback;
    LOG_DBG("Registered keypress sync callback");
}
