#include "nvs.h"
#include "nvs_flash.h"
#include "zigbee.h"
#include "esp_timer.h"

#define NVS_NAMESPACE "zigbee"
#define MAX_DEVICE_NAME_LENGTH 32
#define NVS_COMMIT_TIMEOUT_MS 1000
#define REJOIN_THRESHOLD_MS   5000

// Add these NVS keys
#define NVS_KEY_DEVICE_COUNT "dev_count"
#define NVS_KEY_DEVICE_BASE "device_"

static const char *TAG = "ZIGBEE";
static zigbee_device_t stored_devices[MAX_TRV_DEVICES + MAX_WINDOW_SENSORS];
static uint8_t stored_device_count = 0;
static bool network_open = false;


// Modify device_exists to check last seen time
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

// Modify save_device_to_nvs function
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

esp_err_t clear_zigbee_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_erase_all(handle);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        stored_device_count = 0;
        memset(stored_devices, 0, sizeof(stored_devices));
        ESP_LOGI(TAG, "NVS storage cleared successfully");
    }

    return err;
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
    // TODO: Implement actual device type identification based on your devices
    // For now, just alternate between TRV and window sensor
    if (stored_device_count < MAX_TRV_DEVICES) {
        return DEVICE_TYPE_TRV;
    } else {
        return DEVICE_TYPE_WINDOW_SENSOR;
    }
}

void zigbee_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;

    switch (sig_type) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
            ESP_LOGI(TAG, "Zigbee stack initialized");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
            break;

        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
            ESP_LOGI(TAG, "Starting network formation");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
            break;

        case ESP_ZB_BDB_SIGNAL_FORMATION:
            if (signal_struct->esp_err_status == ESP_OK) {
                ESP_LOGI(TAG, "Network formation successful");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                
                if (nvs_check_for_paired_devices()) {
                    ESP_LOGI(TAG, "Found paired devices, loading config...");
                    load_devices_from_nvs();
                } else {
                    ESP_LOGI(TAG, "No paired devices found, opening network...");
                    open_network(180);
                }
            } else {
                ESP_LOGE(TAG, "Network formation failed");
            }
            break;

        case ESP_ZB_BDB_SIGNAL_STEERING:
            if (signal_struct->esp_err_status == ESP_OK) {
                ESP_LOGI(TAG, "Network steering completed");
            }
            break;

        case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
            dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
            
            if (device_exists(dev_annce_params->device_short_addr)) {
                ESP_LOGI(TAG, "Device 0x%04x rejoined network", dev_annce_params->device_short_addr);
                break;
            }

            ESP_LOGI(TAG, "New device joining network (short: 0x%04x)", 
                    dev_annce_params->device_short_addr);

            device_type_t dev_type = identify_device_type(dev_annce_params);
            if ((dev_type == DEVICE_TYPE_TRV && stored_device_count >= MAX_TRV_DEVICES) ||
                (dev_type == DEVICE_TYPE_WINDOW_SENSOR && stored_device_count >= MAX_WINDOW_SENSORS)) {
                ESP_LOGW(TAG, "Maximum devices of this type reached");
                return;
            }

            zigbee_device_t new_device = {
                .type = dev_type,
                .short_addr = dev_annce_params->device_short_addr,
                .endpoint = 1
            };

            snprintf(new_device.name, MAX_DEVICE_NAME_LENGTH, "%s_%d", 
                    dev_type == DEVICE_TYPE_TRV ? "TRV" : "WINDOW", 
                    stored_device_count + 1);

            if (save_device_to_nvs(&new_device) == ESP_OK) {
                ESP_LOGI(TAG, "Device saved as %s", new_device.name);
                save_devices_to_nvs();  // Save all devices to persistent storage
                if (stored_device_count >= (MAX_TRV_DEVICES + MAX_WINDOW_SENSORS)) {
                    close_network();
                }
            }
            break;

        case ESP_ZB_ZDO_SIGNAL_LEAVE:
            ESP_LOGW(TAG, "Device left the network");
            break;

        case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:  // Signal 6
            ESP_LOGI(TAG, "Permit join status changed");
            break;

        case ESP_ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:  // Signal 54
            ESP_LOGD(TAG, "Production config ready");
            break;

        case ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED:  // Signal 48
            ESP_LOGI(TAG, "Device authorized");
            break;

        // Common signals that don't need specific handling
        case ESP_ZB_COMMON_SIGNAL_CAN_SLEEP:
            ESP_LOGD(TAG, "Stack can sleep");
            break;

        case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION:
            ESP_LOGI(TAG, "Leave indication received");
            break;

        case ESP_ZB_BDB_FINDING_N_BINDING:
            ESP_LOGD(TAG, "Finding and binding signal");
            break;

        default:
            ESP_LOGW(TAG, "Unhandled Zigbee signal: %d", sig_type);
            break;
    }
}
