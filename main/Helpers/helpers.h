


#include "esp_log.h"

#include "zcl/esp_zigbee_zcl_thermostat.h"
#include "zcl/esp_zigbee_zcl_ias_zone.h"

#include "Zigbee/zigbee.h"

void show_stored_devices(void);
char* get_device_name(uint16_t address);
void turn_trvs_off(void);
void turn_trvs_on(void);