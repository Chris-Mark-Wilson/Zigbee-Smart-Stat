#include "Zigbee/zigbee.h"
#include "esp_zigbee_core.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "Helpers/helpers.h"
#include "esp_timer.h"

#define NVS_NAMESPACE "zigbee"
#define MAX_DEVICE_NAME_LENGTH 32
#define NVS_COMMIT_TIMEOUT_MS 1000

#define NVS_KEY_DEVICE_COUNT "dev_count"
#define NVS_KEY_DEVICE_BASE "device_"


esp_err_t save_device_to_nvs(zigbee_device_t *device)
{
    if(device_exists(device->short_addr))
    {
        ESP_LOGI("Save device to nvs helper", "Device already exists in NVS");
        return ESP_OK;
    }
    static int64_t last_save = 0;
    int64_t now = esp_timer_get_time() / 1000; // Convert to ms

    // First check if device already exists
    if (device_exists(device->short_addr))
    {
        // Update last seen time for existing device
        for (uint8_t i = 0; i < stored_device_count; i++)
        {
            if (stored_devices[i].short_addr == device->short_addr)
            {
                stored_devices[i].last_seen = now;
                ESP_LOGI("save device to nvs helper", "Updated last seen time for device 0x%04x", device->short_addr);
                break;
            }
        }
        return ESP_OK;
    }

    // Prevent rapid successive writes to NVS
    if ((now - last_save) < NVS_COMMIT_TIMEOUT_MS)
    {
        ESP_LOGW("save device to nvs helper", "Too soon to save to NVS, skipping");
        return ESP_OK;
    }

    if (stored_device_count >= (MAX_TRV_DEVICES + MAX_WINDOW_SENSORS))
    {
        ESP_LOGE("save device to nvs helper", "Maximum device limit reached");
        return ESP_ERR_NO_MEM;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE("NVS Helper functions", "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Save new device count
    err = nvs_set_u8(handle, NVS_KEY_DEVICE_COUNT, stored_device_count + 1);
    if (err != ESP_OK)
    {
        ESP_LOGE("NVS Helper functions", "Error saving device count: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Create unique key for this device
    char key[16];
    snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEVICE_BASE, stored_device_count);

    // Save device data
    err = nvs_set_blob(handle, key, device, sizeof(zigbee_device_t));
    if (err != ESP_OK)
    {
        ESP_LOGE("NVS Helper functions", "Error saving device data: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK)
    {
        device->last_seen = now;
        stored_devices[stored_device_count] = *device;
        stored_device_count++;
        last_save = now;
        ESP_LOGI("Save device to nvs helper", "Device saved successfully: %s", device->name);
    }

    return err;
}

esp_err_t clear_all_nvs(void)
{
    // Clear default NVS partition
    nvs_flash_erase();
    nvs_flash_init();

    // Clear Zigbee storage partition
    const esp_partition_t *zb_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "zb_storage");
    if (zb_partition)
    {
        esp_partition_erase_range(zb_partition, 0, zb_partition->size);
    }

    // Clear Zigbee factory partition
    const esp_partition_t *zb_fct_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "zb_fct");
    if (zb_fct_partition)
    {
        esp_partition_erase_range(zb_fct_partition, 0, zb_fct_partition->size);
    }
    trv_count = 0;
    window_sensor_count = 0;
    stored_device_count = 0;
    return ESP_OK;
}


bool nvs_check_for_paired_devices(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
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
    if (err != ESP_OK)
        return err;

    // Load device count
    err = nvs_get_u8(handle, NVS_KEY_DEVICE_COUNT, &stored_device_count);
    if (err != ESP_OK)
    {
        stored_device_count = 0;
        nvs_close(handle);
        return err;
    }

    // Load each device
    for (uint8_t i = 0; i < stored_device_count; i++)
    {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEVICE_BASE, i);
        size_t size = sizeof(zigbee_device_t);
        err = nvs_get_blob(handle, key, &stored_devices[i], &size);
        if (err != ESP_OK)
        {
            ESP_LOGW("NVS Helper functions", "Failed to load device %d", i);
            continue;
        }
        ESP_LOGI("NVS Helper functions", "Loaded device: %s (0x%04x)",
                 stored_devices[i].name, stored_devices[i].short_addr);
    }

    nvs_close(handle);
    return ESP_OK;
}
