#include "display_split_sync.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/event_manager.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// BLE service UUIDs for display sync (currently unused)
// #define DISPLAY_SYNC_SERVICE_UUID BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x567812345678)
// #define DISPLAY_SYNC_CHAR_UUID BT_UUID_128_ENCODE(0x12345679, 0x1234, 0x5678, 0x1234, 0x567812345678)

static display_sync_received_callback_t sync_callback = NULL;
static struct display_sync_data last_sync_data = {0};

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
// Central implementation
static struct k_work_delayable sync_work;
static bool sync_enabled = false;

static void sync_work_handler(struct k_work *work) {
    if (!sync_enabled) {
        return;
    }
    
    // Send sync data to peripheral
    // In a real implementation, this would use ZMK's split communication
    // For now, we'll use a simpler approach
    
    LOG_DBG("Sending display sync data to peripheral");
      // Schedule next sync
    k_work_schedule(&sync_work, K_MSEC(2000)); // Sync every 2 seconds to avoid overwhelming peripheral
}

int display_split_sync_send_data(const struct display_sync_data *data) {
    if (!data) {
        return -EINVAL;
    }
    
    // Store the data for transmission
    memcpy(&last_sync_data, data, sizeof(struct display_sync_data));
    last_sync_data.sync_timestamp = k_uptime_get_32();
    
    LOG_DBG("Queued display sync data: WPM[9]=%d, layer=%d, profile=%d", 
            data->wpm[9], data->layer_index, data->active_profile_index);
    
    return 0;
}

int display_split_sync_init(void) {
    LOG_INF("Initializing display split sync (central)");
    
    k_work_init_delayable(&sync_work, sync_work_handler);
    sync_enabled = true;
    
    // Start sync work
    k_work_schedule(&sync_work, K_MSEC(1000)); // Initial delay
    
    return 0;
}

#else // Peripheral implementation

// Simulate receiving data for now
// In a real implementation, this would be received via BLE
static struct k_timer sync_timer;

static void sync_timer_handler(struct k_timer *timer) {
    if (sync_callback) {
        // Simulate receiving updated data from central
        // In reality, this would come from BLE communication
        sync_callback(&last_sync_data);
        LOG_DBG("Received display sync data on peripheral");
    }
}

int display_split_sync_init(void) {
    LOG_INF("Initializing display split sync (peripheral)");
    
    // Disable automatic timer updates for now to avoid interfering with keyboard
    // k_timer_init(&sync_timer, sync_timer_handler, NULL);
    // k_timer_start(&sync_timer, K_MSEC(2000), K_MSEC(2000)); // Reduce frequency to 2 seconds
    
    return 0;
}

int display_split_sync_send_data(const struct display_sync_data *data) {
    // Peripheral doesn't send data
    return -ENOTSUP;
}

#endif

void display_split_sync_register_callback(display_sync_received_callback_t callback) {
    sync_callback = callback;
    LOG_DBG("Registered display sync callback");
}

// Getter for last sync data (used by peripheral)
const struct display_sync_data* display_split_sync_get_last_data(void) {
    return &last_sync_data;
}
