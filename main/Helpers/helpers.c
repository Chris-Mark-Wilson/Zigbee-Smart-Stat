#include "helpers.h"
#include "Zigbee/zigbee.h"
#include "ui_events.h"
#include "esp_zigbee_core.h"
#include "esp_log.h"
#include "main.h"

#define REJOIN_THRESHOLD_MS 30000

void show_stored_devices(void)
{

    for (uint8_t i = 0; i < stored_device_count; i++)
    {
        ESP_LOGI("DEVICES", "Device %d: %s (0x%04x), type: %s", i, stored_devices[i].name, stored_devices[i].short_addr,stored_devices[i].type == DEVICE_TYPE_TRV ? "TRV" : "Window Sensor");
    }
}
  


char* get_device_name(uint16_t address)
{
    static char device_name[32];  // Static buffer to hold the name
    
    // Find device in stored devices array
    for (uint8_t i = 0; i < stored_device_count; i++) {
        if (stored_devices[i].short_addr == address) {
            return stored_devices[i].name;
        }
    }
    
    // If not found, return generic name
    snprintf(device_name, sizeof(device_name), "Unknown (0x%04x)", address);
    return device_name;
}

 void turn_trvs_off(void)
{
    for (uint8_t i = 0; i < stored_device_count; i++)
    {
        if (stored_devices[i].type == DEVICE_TYPE_TRV)
        {
            esp_zb_zcl_write_attr_cmd_t mode_cmd = {
                .zcl_basic_cmd = {
                    .dst_addr_u.addr_short = stored_devices[i].short_addr,
                    .dst_endpoint = stored_devices[i].endpoint,
                    .src_endpoint = HA_THERMOSTAT_ENDPOINT,
                },
                .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                .clusterID = ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT,
                .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
                .attr_number = 1,
                .attr_field = &(esp_zb_zcl_attribute_t){.id = ESP_ZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID,
                .data.type = ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, // Use 8-bit enum type
                .data.size = sizeof(uint8_t),
                .data.value = (uint8_t *)(&(uint8_t){ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_OFF})}};
         ESP_LOGI("TRV OPERATION", "Turning OFF TRV at address 0x%04x", stored_devices[i].short_addr);
            esp_zb_zcl_write_attr_cmd_req(&mode_cmd);
        }
    }
    g_trv_state=false;
}
void turn_trvs_on(void)
{
    for (uint8_t i = 0; i < stored_device_count; i++)
    {
        if (stored_devices[i].type == DEVICE_TYPE_TRV)
        {
            // Create a static value to ensure it remains valid
            static uint8_t mode_value = ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_HEAT;
            
            esp_zb_zcl_write_attr_cmd_t mode_cmd = {
                .zcl_basic_cmd = {
                    .dst_addr_u.addr_short = stored_devices[i].short_addr,
                    .dst_endpoint = stored_devices[i].endpoint,
                    .src_endpoint = HA_THERMOSTAT_ENDPOINT,
                },
                .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                .clusterID = ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT,
                .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
                .attr_number = 1,
                .attr_field = &(esp_zb_zcl_attribute_t){
                    .id = ESP_ZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID,
                    .data.type = ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM,
                    .data.size = sizeof(uint8_t),
                    .data.value = &mode_value
                }
            };

            ESP_LOGI("TRV OPERATION", "Turning ON TRV at address 0x%04x", stored_devices[i].short_addr);
            esp_zb_zcl_write_attr_cmd_req(&mode_cmd);
        }
    }
    g_trv_state=true;
}
void display_network_key(void)
{
    uint8_t network_key[16];
    esp_err_t status = esp_zb_secur_primary_network_key_get(network_key);

    if (status == ESP_OK)
    {
        ESP_LOGI("Display netwrok key helper", "Zigbee Network Key: ");
        for (int i = 0; i < 16; i++)
        {
            printf("%02x", network_key[i]);
            if (i < 15)
                printf(":");
        }
        printf("\n");
    }
    else
    {
        ESP_LOGW("Display netwrok key helper", "Failed to get network key, error: %s", esp_err_to_name(status));
        ESP_LOGW("Display network key helper", "Note: Network key can only be obtained after the device has joined the network");
    }
}

void ui_display_message(const char *message)
{
    ui_event_t event = {
        .target_screen = SCREEN_BOOT,
        .message = ""};
    strncpy(event.message, message, sizeof(event.message) - 1);
    xQueueSend(ui_event_queue, &event, 0);
}

bool device_exists(uint16_t short_addr)
{
    int64_t now = esp_timer_get_time() / 1000;

    for (uint8_t i = 0; i < stored_device_count; i++)
    {
        if (stored_devices[i].short_addr == short_addr)
        {
            // Check if device was seen recently
            if ((now - stored_devices[i].last_seen) > REJOIN_THRESHOLD_MS)
            {
                // Remove device by shifting remaining elements
                if (i < stored_device_count - 1) {
                    // Use memcpy for better performance with structs
                    memcpy(&stored_devices[i], 
                           &stored_devices[i + 1], 
                           sizeof(zigbee_device_t) * (stored_device_count - i - 1));
                }
                stored_device_count--;
                if(stored_devices[i].type == DEVICE_TYPE_TRV)
                {
                    trv_count--;
                }
                else if(stored_devices[i].type == DEVICE_TYPE_WINDOW_SENSOR)
                {
                    window_sensor_count--;
                }
                // Clear the last element to avoid dangling pointer
                memset(&stored_devices[stored_device_count], 0, sizeof(zigbee_device_t));
                // Log the removal
                ESP_LOGI("device_exists", "Device 0x%04x expired and removed, new count: %d", 
                         short_addr, stored_device_count);
                return false;
            }
            return true;
        }
    }
    return false;
}