#include "helpers.h"
#include "Zigbee/zigbee.h"

#include "esp_zigbee_core.h"



void show_stored_devices(void)
{

    for (uint8_t i = 0; i < stored_device_count; i++)
    {
        ESP_LOGI("DEVICES", "Device %d: %s (0x%04x)", i, stored_devices[i].name, stored_devices[i].short_addr);
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
}