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
#define IAS_ZONE_ENDPOINT               1           /* esp IAS zone device endpoint */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     (1l << 13)  /* Zigbee primary channel mask use in the example */

/* Attribute values in ZCL string format
 * The string should be started with the length of its own.
 */
#define MANUFACTURER_NAME               "\x09""TUYA" //trying toi see if this helps wqith tuya stuff
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
#define MAX_DEVICE_NAME_LENGTH  32
#define MAX_TRV_DEVICES         2
#define MAX_WINDOW_SENSORS      3
#define LONG_DELAY              pdMS_TO_TICKS(3000)
#define SHORT_DELAY             pdMS_TO_TICKS(1000)
#define MAX_UNPAIR_ATTEMPTS     3
#define MAX_TRV_DEVICES         2
#define MAX_WINDOW_SENSORS      3
#define MAX_DEVICE_NAME_LENGTH  32
#define PAIRING_MODE_UI_MESSAGE "Pairing Mode: ON \nPress button when finished"

// Device type enumeration
typedef enum {
    DEVICE_TYPE_UNKNOWN = 0,
    DEVICE_TYPE_TRV = 1,
    DEVICE_TYPE_WINDOW_SENSOR = 2
} device_type_t;

// Device structure definition
typedef struct {
    esp_zb_ieee_addr_t ieee_addr; // IEEE address of the device
    device_type_t type;
    uint16_t short_addr;
    uint8_t endpoint;
    char name[MAX_DEVICE_NAME_LENGTH];
    int64_t last_seen;  // Timestamp for device tracking
    uint8_t unpair_attempts;
} zigbee_device_t;


// Global state tracking variables
extern bool g_is_window_open; // Global window state
extern bool network_open;
extern uint8_t stored_device_count;
extern uint8_t trv_count;
extern uint8_t window_sensor_count;

// Declare external array
extern zigbee_device_t stored_devices[MAX_TRV_DEVICES + MAX_WINDOW_SENSORS];

// Function declarations
void zigbee_signal_handler(esp_zb_app_signal_t *signal_struct);
void esp_zb_zcl_config_report_cb(esp_zb_zcl_command_send_status_message_t message);
esp_err_t clear_all_nvs(void);

void read_window_sensor_status(uint16_t addr, uint8_t endpoint);

esp_err_t close_network(void);

void display_network_key(void);

void ui_display_message(const char *message);

void reset_device(void);
void unpair_device(uint16_t short_addr);

void (reset_device_cb)(esp_zb_zdp_status_t zdo_status, void *user_ctx);
void restart(void);

zigbee_device_t *get_device_by_short_addr(uint16_t short_addr);

#endif // ZIGBEE_H