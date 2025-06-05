#pragma once

#include <zephyr/types.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

// UUIDs must match those in your Python script
#define CUSTOM_STATS_SERVICE_UUID BT_UUID_DECLARE_128(0xe72063c8, 0x961b, 0x4111, 0xb757, 0x5147b859b6ea)
#define CUSTOM_STATS_CHAR_UUID    BT_UUID_DECLARE_128(0x9a89ab4a, 0x340d, 0x4140, 0xb6f0, 0xdb8a50c0604d)

// Function to initialize the service
void custom_stats_service_init(void);

// Function to update stats (if needed)
void custom_stats_service_update(const char *stats, size_t len);
