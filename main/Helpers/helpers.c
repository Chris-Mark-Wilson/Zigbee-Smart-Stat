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
            static int16_t off_setpoint = 500;   // 5.00°C → fully closes valve

            esp_zb_zcl_write_attr_cmd_t cmd = {
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
                    .id = ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID,
                    .data.type = ESP_ZB_ZCL_ATTR_TYPE_S16,
                    .data.size = sizeof(int16_t),
                    .data.value = (uint8_t *)&off_setpoint
                }
            };

            ESP_LOGI("TRV OPERATION", "Turning OFF TRV at address 0x%04x", stored_devices[i].short_addr);
            esp_zb_zcl_write_attr_cmd_req(&cmd);
        }
    }

    g_trv_state = false;
}


void turn_trvs_on(void)
{
    for (uint8_t i = 0; i < stored_device_count; i++)
    {
        if (stored_devices[i].type == DEVICE_TYPE_TRV)
        {
            uint16_t addr = stored_devices[i].short_addr;
            uint8_t ep   = stored_devices[i].endpoint;

            /* ---- Step 1: SET SYSTEM MODE = HEAT ---- */
            static uint8_t heat_mode = ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_HEAT;

            esp_zb_zcl_write_attr_cmd_t sysmode_cmd = {
                .zcl_basic_cmd = {
                    .dst_addr_u.addr_short = addr,
                    .dst_endpoint = ep,
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
                    .data.value = &heat_mode
                }
            };

            ESP_LOGI("TRV OPERATION", "TRV %04x: Setting SystemMode=HEAT", addr);
            esp_zb_zcl_write_attr_cmd_req(&sysmode_cmd);

            vTaskDelay(pdMS_TO_TICKS(150));  // ✔️ Sonoff TRV requires spacing

            /* ---- Step 2: SET HEATING SETPOINT ---- */
            static int16_t setpoint_value = 3000;  // 30.00 C (×100)

            esp_zb_zcl_write_attr_cmd_t setpoint_cmd = {
                .zcl_basic_cmd = {
                    .dst_addr_u.addr_short = addr,
                    .dst_endpoint = ep,
                    .src_endpoint = HA_THERMOSTAT_ENDPOINT,
                },
                .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                .clusterID = ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT,
                .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
                .attr_number = 1,
                .attr_field = &(esp_zb_zcl_attribute_t){
                    .id = ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID,
                    .data.type = ESP_ZB_ZCL_ATTR_TYPE_S16,
                    .data.size = sizeof(int16_t),
                    .data.value = (uint8_t *)&setpoint_value
                }
            };

            ESP_LOGI("TRV OPERATION",
                     "TRV %04x: Setting Heating Setpoint = %.2f°C",
                     addr, setpoint_value / 100.0);

            esp_zb_zcl_write_attr_cmd_req(&setpoint_cmd);
        }
    }

    g_trv_state = true;
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
void mark_device_seen(uint16_t short_addr)
{
    int64_t now = esp_timer_get_time() / 1000;

    for (uint8_t i = 0; i < stored_device_count; i++)
    {
        if (stored_devices[i].short_addr == short_addr)
        {
            stored_devices[i].last_seen = now;
            ESP_LOGD("HELPERS", "Marked device 0x%04x as seen", short_addr);
            return;
        }
    }
}

bool device_exists(uint16_t addr) {
    for (int i = 0; i < stored_device_count; i++) {
        if (stored_devices[i].short_addr == addr) {
            return true;
        }
    }
    return false;
}

void initialise_trv(uint16_t addr, uint8_t ep) {
    // System mode = HEAT
    static uint8_t sys_heat = ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_HEAT;

    esp_zb_zcl_write_attr_cmd_t mode_cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = addr,
            .dst_endpoint = ep,
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
            .data.value = &sys_heat,
        }
    };

    esp_zb_zcl_write_attr_cmd_req(&mode_cmd);

    // Write a safe default setpoint (e.g., 2000 = 20°C)
    static int16_t sp = 2000;

    esp_zb_zcl_write_attr_cmd_t sp_cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = addr,
            .dst_endpoint = ep,
            .src_endpoint = HA_THERMOSTAT_ENDPOINT,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .attr_number = 1,
        .attr_field = &(esp_zb_zcl_attribute_t){
            .id = ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID,
            .data.type = ESP_ZB_ZCL_ATTR_TYPE_S16,
            .data.size = sizeof(int16_t),
            .data.value = (uint8_t *)&sp
        }
    };

    esp_zb_zcl_write_attr_cmd_req(&sp_cmd);
}

