// split_sync.c - Propagate WPM, layer, and profile from central to peripheral
#include <zmk/split/central.h>
#include <zmk/split/peripheral.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/event_manager.h>
#include <zephyr/kernel.h>
#include <string.h>

struct split_sync_state {
    uint8_t wpm;
    uint8_t layer;
    uint8_t profile;
};

static struct split_sync_state sync_state = {0};

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static int split_sync_event_listener(const zmk_event_t *eh) {
    bool changed = false;
    if (as_zmk_wpm_state_changed(eh)) {
        sync_state.wpm = as_zmk_wpm_state_changed(eh)->state;
        changed = true;
    }
    if (as_zmk_layer_state_changed(eh)) {
        sync_state.layer = as_zmk_layer_state_changed(eh)->layer;
        changed = true;
    }
    if (as_zmk_ble_active_profile_changed(eh)) {
        sync_state.profile = as_zmk_ble_active_profile_changed(eh)->index;
        changed = true;
    }
    if (changed) {
        // TODO: Implement split data send using ZMK split transport API
        // Example: zmk_split_central_send_custom_data((uint8_t *)&sync_state, sizeof(sync_state));
    }
    return 0;
}

ZMK_LISTENER(split_sync, split_sync_event_listener);
ZMK_SUBSCRIPTION(split_sync, zmk_wpm_state_changed);
ZMK_SUBSCRIPTION(split_sync, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(split_sync, zmk_ble_active_profile_changed);
#endif

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_PERIPHERAL)
// TODO: Implement split data receive using ZMK split transport API
// void zmk_split_peripheral_receive_custom_data(const uint8_t *data, size_t len) {
//     if (len == sizeof(sync_state)) {
//         memcpy(&sync_state, data, sizeof(sync_state));
//     }
// }
#endif

// Provide accessors for your widgets
uint8_t split_sync_get_wpm(void) { return sync_state.wpm; }
uint8_t split_sync_get_layer(void) { return sync_state.layer; }
uint8_t split_sync_get_profile(void) { return sync_state.profile; }
