#include "nvs.h"
#include "nvs_flash.h"
#include "zigbee.h"
#include "esp_timer.h"
#include "../LVGL_UI/ui_screens.h"
#include "../LVGL_UI/ui_events.h"

#define NVS_NAMESPACE "zigbee"
#define MAX_DEVICE_NAME_LENGTH 32
#define NVS_COMMIT_TIMEOUT_MS 1000
#define REJOIN_THRESHOLD_MS   5000


#define NVS_KEY_DEVICE_COUNT "dev_count"
#define NVS_KEY_DEVICE_BASE "device_"

static const char *TAG = "ZIGBEE";
static zigbee_device_t stored_devices[MAX_TRV_DEVICES + MAX_WINDOW_SENSORS];
static uint8_t stored_device_count = 0;
static uint8_t trv_count = 0;
static uint8_t window_sensor_count = 0;
bool network_open = false;

extern QueueHandle_t ui_event_queue;

static bool device_exists(uint16_t short_addr) 
{
    int64_t now = esp_timer_get_time() / 1000;
    
    for (uint8_t i = 0; i < stored_device_count; i++) {
        if (stored_devices[i].short_addr == short_addr) {
            // Check if device was seen recently
            if ((now - stored_devices[i].last_seen) > REJOIN_THRESHOLD_MS) {
                ESP_LOGI(TAG, "Device 0x%04x expired, treating as new join", short_addr);
                return false;
            }
            return true;
        }
    }
    return false;
}

esp_err_t save_device_to_nvs(zigbee_device_t *device) 
{
    static int64_t last_save = 0;
    int64_t now = esp_timer_get_time() / 1000;  // Convert to ms

    // First check if device already exists
    if (device_exists(device->short_addr)) {
        // Update last seen time for existing device
        for (uint8_t i = 0; i < stored_device_count; i++) {
            if (stored_devices[i].short_addr == device->short_addr) {
                stored_devices[i].last_seen = now;
                ESP_LOGI(TAG, "Updated last seen time for device 0x%04x", device->short_addr);
                break;
            }
        }
        return ESP_OK;
    }

    // Prevent rapid successive writes to NVS
    if ((now - last_save) < NVS_COMMIT_TIMEOUT_MS) {
        ESP_LOGW(TAG, "Too soon to save to NVS, skipping");
        return ESP_OK;
    }

    if (stored_device_count >= (MAX_TRV_DEVICES + MAX_WINDOW_SENSORS)) {
        ESP_LOGE(TAG, "Maximum device limit reached");
        return ESP_ERR_NO_MEM;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Save new device count
    err = nvs_set_u8(handle, NVS_KEY_DEVICE_COUNT, stored_device_count + 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving device count: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Create unique key for this device
    char key[16];
    snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEVICE_BASE, stored_device_count);
    
    // Save device data
    err = nvs_set_blob(handle, key, device, sizeof(zigbee_device_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving device data: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        device->last_seen = now;
        stored_devices[stored_device_count] = *device;
        stored_device_count++;
        last_save = now;
        ESP_LOGI(TAG, "Device saved successfully: %s", device->name);
    }

    return err;
}

esp_err_t save_devices_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    // Save device count
    err = nvs_set_u8(handle, NVS_KEY_DEVICE_COUNT, stored_device_count);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    // Save each device
    for (uint8_t i = 0; i < stored_device_count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEVICE_BASE, i);
        err = nvs_set_blob(handle, key, &stored_devices[i], sizeof(zigbee_device_t));
        if (err != ESP_OK) {
            nvs_close(handle);
            return err;
        }
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

void clear_all_nvs(void)
{
    // Clear default NVS partition
    nvs_flash_erase();
    nvs_flash_init();
    
    // Clear Zigbee storage partition
    const esp_partition_t* zb_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "zb_storage");
    if (zb_partition) {
        esp_partition_erase_range(zb_partition, 0, zb_partition->size);
    }
    
    // Clear Zigbee factory partition
    const esp_partition_t* zb_fct_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "zb_fct");
    if (zb_fct_partition) {
        esp_partition_erase_range(zb_fct_partition, 0, zb_fct_partition->size);
    }
    trv_count = 0;
    window_sensor_count = 0;
    stored_device_count = 0;
}

bool nvs_check_for_paired_devices(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    uint8_t count = 0;
    err = nvs_get_u8(handle, NVS_KEY_DEVICE_COUNT, &count);
    nvs_close(handle);

    return (err == ESP_OK && count > 0);
}

esp_err_t load_devices_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    // Load device count
    err = nvs_get_u8(handle, NVS_KEY_DEVICE_COUNT, &stored_device_count);
    if (err != ESP_OK) {
        stored_device_count = 0;
        nvs_close(handle);
        return err;
    }

    // Load each device
    for (uint8_t i = 0; i < stored_device_count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEVICE_BASE, i);
        size_t size = sizeof(zigbee_device_t);
        err = nvs_get_blob(handle, key, &stored_devices[i], &size);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to load device %d", i);
            continue;
        }
        ESP_LOGI(TAG, "Loaded device: %s (0x%04x)", 
                stored_devices[i].name, stored_devices[i].short_addr);
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t open_network(uint16_t duration)
{
    if (esp_zb_bdb_open_network(duration) != ESP_OK) {
        return ESP_FAIL;
    }
    network_open = true;
    ESP_LOGI(TAG, "Network opened for %d seconds", duration);
    return ESP_OK;
}

esp_err_t close_network(void)
{
    if (esp_zb_bdb_close_network() != ESP_OK) {
        return ESP_FAIL;
    }
    network_open = false;
    ESP_LOGI(TAG, "Network closed");
    return ESP_OK;
}

bool is_network_open(void)
{
    return network_open;
}

static device_type_t identify_device_type(esp_zb_zdo_signal_device_annce_params_t *params)
{
    ESP_LOGI(TAG, "Identifying device type for device 0x%04x", params->device_short_addr);
    ESP_LOGI(TAG, "Capabilities: 0x%02x", params->capability);
    ESP_LOGI(TAG, "MAC Address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
             params->ieee_addr[7], params->ieee_addr[6], params->ieee_addr[5], params->ieee_addr[4],
             params->ieee_addr[3], params->ieee_addr[2], params->ieee_addr[1], params->ieee_addr[0]);

    // From your logs:
    // Window sensor MAC: a4:c1:38:7e:04:08:55:c2
    // TRV MAC: 34:10:f4:ff:fe:e1:90:f3
    
    // Identify TRV by its unique MAC prefix
    if (params->ieee_addr[7] == 0x34 && params->ieee_addr[6] == 0x10) {
        ESP_LOGI(TAG, "Identified TRV by MAC address prefix");
        return DEVICE_TYPE_TRV;
    }
    
    // Identify Window sensor by its unique MAC prefix
    if (params->ieee_addr[7] == 0xa4 && params->ieee_addr[6] == 0xc1) {
        ESP_LOGI(TAG, "Identified Window Sensor by MAC address prefix");
        return DEVICE_TYPE_WINDOW_SENSOR;
    }

    // Fallback identification using raw capability byte
    ESP_LOGI(TAG, "MAC address not recognized, using raw capability byte: 0x%02x", params->capability);
    ESP_LOGI(TAG, "Raw capabilities breakdown:");
    ESP_LOGI(TAG, "Bit 7 (0x80): %d", (params->capability & 0x80) ? 1 : 0);
    ESP_LOGI(TAG, "Bit 6 (0x40): %d", (params->capability & 0x40) ? 1 : 0);
    ESP_LOGI(TAG, "Bit 5 (0x20): %d", (params->capability & 0x20) ? 1 : 0);
    ESP_LOGI(TAG, "Bit 4 (0x10): %d", (params->capability & 0x10) ? 1 : 0);
    ESP_LOGI(TAG, "Bit 3 (0x08): %d", (params->capability & 0x08) ? 1 : 0);
    ESP_LOGI(TAG, "Bit 2 (0x04): %d", (params->capability & 0x04) ? 1 : 0);
    ESP_LOGI(TAG, "Bit 1 (0x02): %d", (params->capability & 0x02) ? 1 : 0);
    ESP_LOGI(TAG, "Bit 0 (0x01): %d", (params->capability & 0x01) ? 1 : 0);

    // Default to Window Sensor as fallback
    ESP_LOGW(TAG, "Could not definitively identify device type, defaulting to Window Sensor");
    return DEVICE_TYPE_WINDOW_SENSOR;
}

void zigbee_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;

    switch (sig_type) {
 


            case ESP_ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY: {  // Signal 23 (0x17)
                ui_event_t event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "Starting Zigbee stack..."
                };
                xQueueSend(ui_event_queue, &event, portMAX_DELAY);
                ESP_LOGI(TAG, "Production config ready, starting initialization");
                break;
            }

            case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP: {  // Signal 6 (0x06)
                ESP_LOGI(TAG, "Skip startup signal received with status: 0x%x", signal_struct->esp_err_status);
                ui_event_t event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "Starting network formation..."
                };
                xQueueSend(ui_event_queue, &event, portMAX_DELAY);
                ESP_LOGI(TAG, "Zigbee stack initialized, starting network formation");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
                break;
            }

        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START: {
            ui_event_t event = {
                .target_screen = SCREEN_BOOT,
                .message = "Forming network..."
            };
            xQueueSend(ui_event_queue, &event, 0);
            ESP_LOGI(TAG, "First start, forming network");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
            break;
        }

        case ESP_ZB_BDB_SIGNAL_FORMATION: {
            if (signal_struct->esp_err_status == ESP_OK) {
                ui_event_t event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "Network formed, starting coordinator..."
                };
                xQueueSend(ui_event_queue, &event, 0);
                ESP_LOGI(TAG, "Network formation successful");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ui_event_t event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "Network formation failed"
                };
                xQueueSend(ui_event_queue, &event, 0);
                ESP_LOGE(TAG, "Network formation failed");
            }
            break;
        }
        case ESP_ZB_BDB_SIGNAL_STEERING: {
            if (signal_struct->esp_err_status == ESP_OK) {
                ESP_LOGI(TAG, "Network steering completed");
                if (!nvs_check_for_paired_devices()) {
                    ui_event_t event = {
                        .target_screen = SCREEN_BOOT,
                        .message = "Put devices in pair mode. Press button when finished..."
                    };
                    xQueueSend(ui_event_queue, &event, 0);
                    ESP_LOGI(TAG, "No paired devices found, opening network...");
                    open_network(180);  // Open for 3 minutes
                } else {
                    // ...existing code...
                }
            }
            break;
        }

        case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS: {
            ESP_LOGI(TAG, "Permit join status changed");
            if (network_open) {
                ui_event_t event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "Network open - Add devices then press button when done..."
                };
                xQueueSend(ui_event_queue, &event, 0);
            }
            break;
        }
  
        case ESP_ZB_ZDO_SIGNAL_LEAVE: {
            ui_event_t event = {
                .target_screen = SCREEN_BOOT,
                .message = "Leaving network..."
            };
            xQueueSend(ui_event_queue, &event, portMAX_DELAY);
            ESP_LOGI(TAG, "Device leaving network");
            // Device will restart after this
            break;
        }

        case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION: {
            esp_zb_zdo_signal_leave_indication_params_t *leave_params = 
                (esp_zb_zdo_signal_leave_indication_params_t*)esp_zb_app_signal_get_params(p_sg_p);
            ui_event_t event = {
                .target_screen = SCREEN_BOOT,
                .message = "Device left, restarting..."
            };
            xQueueSend(ui_event_queue, &event, portMAX_DELAY);
            ESP_LOGI(TAG, "Device 0x%04x left network", leave_params->short_addr);
            break;
        }

        case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE: {
            dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
            if (device_exists(dev_annce_params->device_short_addr)) {
                ui_event_t event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "Device rejoined network"
                };
                xQueueSend(ui_event_queue, &event, 0);
                ESP_LOGI(TAG, "Device 0x%04x rejoined network", dev_annce_params->device_short_addr);
                break;
            }

            ESP_LOGI(TAG, "New device joining network (short: 0x%04x)", 
                    dev_annce_params->device_short_addr);

            device_type_t dev_type = identify_device_type(dev_annce_params);
            if ((dev_type == DEVICE_TYPE_TRV && stored_device_count >= MAX_TRV_DEVICES) ||
                (dev_type == DEVICE_TYPE_WINDOW_SENSOR && stored_device_count >= MAX_WINDOW_SENSORS)) {
                ui_event_t event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "Maximum devices of this type reached"
                };
                xQueueSend(ui_event_queue, &event, 0);
                ESP_LOGW(TAG, "Maximum devices of this type reached");
                break;
            }

            zigbee_device_t new_device = {
                .type = dev_type,
                .short_addr = dev_annce_params->device_short_addr,
                .endpoint = 1
            };

            snprintf(new_device.name, MAX_DEVICE_NAME_LENGTH, "%s_%d", 
                dev_type == DEVICE_TYPE_TRV ? "TRV" : "WINDOW", 
                dev_type == DEVICE_TYPE_TRV ? ++trv_count : ++window_sensor_count);

            if (save_device_to_nvs(&new_device) == ESP_OK) {
                ui_event_t event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "Device paired successfully"
                };
                xQueueSend(ui_event_queue, &event, 0);
                ESP_LOGI(TAG, "Device saved as %s", new_device.name);
                
                if (stored_device_count >= (MAX_TRV_DEVICES + MAX_WINDOW_SENSORS)) {
                    close_network();
                    ui_event_t close_event = {
                        .target_screen = SCREEN_BOOT
                    };
                    strncpy(close_event.message, "All devices paired, network closed", 64);
                    xQueueSend(ui_event_queue, &close_event, 0);
                }
            }
            break;
        }

        case ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED: {
            esp_zb_zdo_signal_device_authorized_params_t *auth_params = 
                (esp_zb_zdo_signal_device_authorized_params_t*)esp_zb_app_signal_get_params(p_sg_p);
            
            // Find the device in our stored list
            for (uint8_t i = 0; i < stored_device_count; i++) {
                if (stored_devices[i].short_addr == auth_params->short_addr) {
                    char message[64];
                    snprintf(message, sizeof(message), "Device %s authorized", stored_devices[i].name);
                    ui_event_t event = {
                        .target_screen = SCREEN_BOOT,
                        .message = ""
                    };
                    strncpy(event.message, message, sizeof(event.message) - 1);
                    xQueueSend(ui_event_queue, &event, 0);
                    ESP_LOGI(TAG, "%s", message);
                    break;
                }
            }
            break;
        }
        case ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED:  // 0x12
        ESP_LOGI(TAG, "Device associated with network");
        break;

    case ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE:  // 0x30
        ESP_LOGI(TAG, "Device update notification received");
        break;

    case ESP_ZB_NLME_STATUS_INDICATION:  // 0x32
        ESP_LOGI(TAG, "Device authentication in progress");
        break;
  
 case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT://0x06
        ESP_LOGI(TAG, "Device reboot signal received");
        break;

      
        default:
            ESP_LOGW(TAG, "Unhandled Zigbee signal: %d (0x%x)", sig_type, sig_type);
            break;
    }
}
