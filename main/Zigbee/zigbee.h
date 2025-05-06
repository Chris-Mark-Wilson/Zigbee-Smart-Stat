#ifndef ZIGBEE_H
#define ZIGBEE_H

#include "esp_zigbee_core.h"
#include "nvs.h"
#include "nvs_flash.h"

#define MAX_TRV_DEVICES 2
#define MAX_WINDOW_SENSORS 3
#define MAX_DEVICE_NAME_LENGTH 32

typedef enum {
    DEVICE_TYPE_TRV = 1,
    DEVICE_TYPE_WINDOW_SENSOR = 2
} device_type_t;

// ...existing code...

typedef struct {
    device_type_t type;
    uint16_t short_addr;
    uint8_t endpoint;
    char name[MAX_DEVICE_NAME_LENGTH];
    int64_t last_seen;  // Timestamp for device tracking
} zigbee_device_t;

// ...existing code...

// Function declarations
void zigbee_signal_handler(esp_zb_app_signal_t *signal_struct);
esp_err_t save_device_to_nvs(zigbee_device_t *device);
esp_err_t clear_zigbee_nvs(void);
bool nvs_check_for_paired_devices(void);
esp_err_t load_devices_from_nvs(void);
esp_err_t open_network(uint16_t duration);
esp_err_t close_network(void);
bool is_network_open(void);

#endif // ZIGBEE_H