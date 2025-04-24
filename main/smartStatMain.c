#include <inttypes.h>
#include "esp_log.h"
#include "esp_zigbee_core.h"
#include "nvs_flash.h"

static const char *TAG = "zb_main";

// Zigbee application signal handler
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_s)
{
    if (!signal_s) {
        ESP_LOGE(TAG, "Received null signal");
        return;
    }

    // Extract the signal type and status
    uint32_t *signal_type = signal_s->p_app_signal;
    esp_err_t status = signal_s->esp_err_status;

    // Log the signal type and status
    ESP_LOGI(TAG, "Zigbee signal received: type=%" PRIu32 ", status=%d", *signal_type, status);

    // Handle specific signal types (example)
    switch (*signal_type) {
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
            ESP_LOGI(TAG, "Device reboot signal received");
            break;

        case ESP_ZB_BDB_SIGNAL_STEERING:
            if (status == ESP_OK) {
                ESP_LOGI(TAG, "Device successfully joined the network");
            } else {
                ESP_LOGE(TAG, "Failed to join the network");
            }
            break;

        case ESP_ZB_BDB_SIGNAL_FORMATION:
            if (status == ESP_OK) {
                ESP_LOGI(TAG, "Network formation successful");
            } else {
                ESP_LOGE(TAG, "Network formation failed");
            }
            break;

        default:
            ESP_LOGW(TAG, "Unhandled Zigbee signal: type=%" PRIu32, *signal_type);
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "🧠 Zigbee Coordinator starting up...");
// erase partition before itialising zigbee stack
    // Initialize NVS flash
    esp_err_t ret = nvs_flash_erase_partition("zb_storage");
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "zb_storage partition erased successfully");
} else {
    ESP_LOGE(TAG, "Failed to erase zb_storage partition");
}
ret = nvs_flash_init_partition("zb_storage");
ESP_ERROR_CHECK(ret);

        // Initialize the zb_storage partition
       ret = nvs_flash_init_partition("zb_storage");
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_LOGW(TAG, "Erasing zb_storage partition due to errors...");
            ESP_ERROR_CHECK(nvs_flash_erase_partition("zb_storage"));
            ret = nvs_flash_init_partition("zb_storage");
        }
        ESP_ERROR_CHECK(ret);
        //bit i added to make it stop rebooting on failure to initialise zigbee stack
        if(ret == ESP_OK) {
            ESP_LOGI(TAG, "zb_storage partition initialized successfully");
        } else {
            ESP_LOGE(TAG, "Failed to initialize zb_storage partition");
            return;
        }

    // Initialize Zigbee stack as Coordinator
    esp_zb_cfg_t zb_nwk_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,
        .install_code_policy = false,
        .nwk_cfg.zczr_cfg = {
            .max_children = 10,
        },
    };
    esp_zb_init(&zb_nwk_cfg);

    // Start Zigbee main loop task
    esp_zb_start(false);

    ESP_LOGI(TAG, "📡 Zigbee stack initialized and running");
}
