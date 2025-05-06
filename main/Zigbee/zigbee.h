#ifndef ZIGBEE_H
#define ZIGBEE_H

#include "esp_zigbee_core.h"
#include "nvs.h"
#include "nvs_flash.h"

// Make sure we're using the WEAK attribute for the signal handler
extern void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);

// Other function declarations
void start_commissioning(void);
bool nvs_check_for_paired_devices(void);
void load_devices_from_nvs(void);

#endif // ZIGBEE_H