#include "settings.h"
#include "ui_events.h"
#include "ui_screens.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_err.h"

#define TAG "SETTINGS"

static settings_callback_t settings_callback = NULL;
static SemaphoreHandle_t settings_callback_param = NULL;

void update_range_limit(float new_limit)
{
    if (new_limit < 2.0f) new_limit = 2.0f;
    if (new_limit > 7.0f) new_limit = 7.0f;
    
    g_range_limit = new_limit;
    ESP_LOGI(TAG, "Range limit updated to %.2fm", g_range_limit);
    
    // Force UI update when range limit changes
    ui_event_t event = {
        .target_screen = SCREEN_MAIN,
        .message = ""
    };
    xQueueSend(ui_event_queue, &event, 0);
}
 void settings_complete_cb(SemaphoreHandle_t semaphore)
{
    // This is called when the user finishes setting up
    xSemaphoreGive(semaphore);
    
    // Return to boot screen
    ui_event_t event = {
        .target_screen = SCREEN_BOOT,
        .message = "Settings saved, starting Zigbee network..."
    };
    xQueueSend(ui_event_queue, &event, portMAX_DELAY);
}
bool save_settings(uint32_t channel_mask, uint16_t target_temp, uint16_t min_temp, uint16_t max_temp)
{
    esp_err_t err;
    nvs_handle_t nvs_handle;
    
    err = nvs_open("zigbee", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return false;
    }
    
    // Save channel mask
    err = nvs_set_u32(nvs_handle, "channel_mask", channel_mask);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving channel mask: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    // Save other settings
    err = nvs_set_u16(nvs_handle, "target_temp", target_temp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving target temp: %s", esp_err_to_name(err));
    }
    
    err = nvs_set_u16(nvs_handle, "min_temp", min_temp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving min temp: %s", esp_err_to_name(err));
    }
    
    err = nvs_set_u16(nvs_handle, "max_temp", max_temp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving max temp: %s", esp_err_to_name(err));
    }
    
    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error committing NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    nvs_close(nvs_handle);
    
    // If a callback is registered for settings completion, call it
    if (settings_callback && settings_callback_param) {
        settings_callback(settings_callback_param);
    }
    
    return true;
}

void settings_register_callback(settings_callback_t callback, SemaphoreHandle_t param) {
    settings_callback = callback;
    settings_callback_param = param;
}