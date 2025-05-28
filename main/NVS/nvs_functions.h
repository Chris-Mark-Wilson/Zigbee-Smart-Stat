#ifndef NVS_H
#define NVS_H

#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "Zigbee/zigbee.h"
#include "Helpers/helpers.h"

// Function declarations
esp_err_t save_device_to_nvs(zigbee_device_t *device);
esp_err_t load_devices_from_nvs(void);
esp_err_t clear_all_nvs(void);
bool nvs_check_for_paired_devices(void);

#endif // NVS_H