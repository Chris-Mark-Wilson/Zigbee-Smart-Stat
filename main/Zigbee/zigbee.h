#ifndef ZIGBEE_H
#define ZIGBEE_H

#include "esp_zigbee_core.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "switch_driver.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "zcl/esp_zigbee_zcl_ias_zone.h"

/* Zigbee configuration */
#define MAX_CHILDREN                    10          /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE       false       /* enable the install code policy for security */
#define HA_THERMOSTAT_ENDPOINT          10          /* esp thermostat device endpoint */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     (1l << 13)  /* Zigbee primary channel mask use in the example */

/* Attribute values in ZCL string format
 * The string should be started with the length of its own.
 */
#define MANUFACTURER_NAME               "\x09""ESPRESSIF"
#define MODEL_IDENTIFIER                "\x07"CONFIG_IDF_TARGET

#define ESP_ZB_ZC_CONFIG()                                                              \
    {                                                                                   \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,                                  \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,                               \
        .nwk_cfg.zczr_cfg = {                                                           \
            .max_children = MAX_CHILDREN,                                               \
        },                                                                              \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }
/* End Zigbee configuration*/

// Define constants
#define MAX_DEVICE_NAME_LENGTH 32
#define MAX_TRV_DEVICES 2
#define MAX_WINDOW_SENSORS 3

// Device type enumeration
typedef enum {
    DEVICE_TYPE_UNKNOWN = 0,
    DEVICE_TYPE_TRV = 1,
    DEVICE_TYPE_WINDOW_SENSOR = 2
} device_type_t;

// Device structure definition
typedef struct {
    device_type_t type;
    uint16_t short_addr;
    uint8_t endpoint;
    char name[MAX_DEVICE_NAME_LENGTH];
    int64_t last_seen;  // Timestamp for device tracking
} zigbee_device_t;

// Global state tracking variables
extern bool g_is_window_open; // Global window state

// Declare external array
extern zigbee_device_t stored_devices[MAX_TRV_DEVICES + MAX_WINDOW_SENSORS];
extern uint8_t stored_device_count;
// Function declarations
void zigbee_signal_handler(esp_zb_app_signal_t *signal_struct);
void esp_zb_zcl_config_report_cb(esp_zb_zcl_command_send_status_message_t message);
void clear_all_nvs(void);

void read_window_sensor_status(uint16_t addr, uint8_t endpoint);

esp_err_t close_network(void);
extern bool network_open;

#endif // ZIGBEE_H