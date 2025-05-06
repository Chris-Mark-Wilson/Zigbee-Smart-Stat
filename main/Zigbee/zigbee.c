#include "nvs.h"
#include "nvs_flash.h"
#include "zigbee.h"

// macros for nvs storage
#define MAX_DEVICES 5
#define NVS_NAMESPACE "zigbee"
#define NVS_KEY_FORMAT "dev%d"

typedef enum
{
    DEVICE_TRV,
    DEVICE_WINDOW_SENSOR
} device_type_t;
typedef struct
{
    device_type_t type;
    uint16_t short_addr;
    uint8_t endpoint;
} zigbee_device_t;

zigbee_device_t stored_devices[MAX_DEVICES];
static int stored_device_count = 0;

static const char *TAG = "ZIGBEE";

bool nvs_check_for_paired_devices()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("zigbee", NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return false;
    }

    uint32_t count = 0;
    err = nvs_get_u32(handle, "dev_count", &count);
    nvs_close(handle);

    return (err == ESP_OK && count > 0);
}

void load_devices_from_nvs()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("zigbee", NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGW("NVS", "Failed to open NVS for reading");
        return;
    }

    uint32_t count = 0;
    err = nvs_get_u32(handle, "dev_count", &count);
    if (err != ESP_OK || count > MAX_DEVICES)
    {
        ESP_LOGW("NVS", "Invalid or missing device count");
        nvs_close(handle);
        return;
    }

    for (unsigned int i = 0; i < count; i++)
    {
        char key[16];
        snprintf(key, sizeof(key), "dev_%u", i);
        zigbee_device_t dev;

        size_t len = sizeof(dev);
        err = nvs_get_blob(handle, key, &dev, &len);
        if (err == ESP_OK)
        {
            stored_devices[i] = dev;
            ESP_LOGI("NVS", "Loaded device %u: type=%d addr=0x%04x ep=%d",
                     i, dev.type, dev.short_addr, dev.endpoint);
        }
        else
        {
            ESP_LOGW("NVS", "Failed to load device %u", i);
        }
    }

    stored_device_count = count;
    nvs_close(handle);
}

void start_commissioning(void)
{
    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_NETWORK_STEERING);
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;

    switch (sig_type)
    {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized, skipping default startup.");

        if (nvs_check_for_paired_devices())
        {
            ESP_LOGI(TAG, "Paired device config found. Resuming control logic.");
            load_devices_from_nvs();
            // Optionally trigger discovery of devices to confirm presence
        }
        else
        {
            ESP_LOGI(TAG, "No paired devices found. Starting commissioning...");
            start_commissioning(); // calls esp_zb_bdb_start_top_level_commissioning
        }
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        ESP_LOGI(TAG, "Device started forming the network.");
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        ESP_LOGI(TAG, "Rejoined after reboot.");
        break;

    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->device_short_addr);
        esp_zb_zdo_match_desc_req_param_t cmd_req;
        cmd_req.dst_nwk_addr = dev_annce_params->device_short_addr;
        cmd_req.addr_of_interest = dev_annce_params->device_short_addr;
        ESP_LOGI(TAG, "Sending Match Descriptor Request to device 0x%04hx", cmd_req.dst_nwk_addr);

        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (signal_struct->esp_err_status == ESP_OK)
        {
            ESP_LOGI(TAG, "Device joined successfully.");
        }
        else
        {
            ESP_LOGW(TAG, "Device join failed or timed out.");
        }
        break;

    case ESP_ZB_ZDO_SIGNAL_LEAVE:
        ESP_LOGW(TAG, "A device has left the network.");
        break;

    default:
        ESP_LOGW(TAG, "Unhandled Zigbee signal: %d", sig_type);
        break;
    }


}
