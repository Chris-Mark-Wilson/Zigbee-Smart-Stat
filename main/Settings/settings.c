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

static struct {
    uint8_t high_temp;
    uint8_t low_temp;
    float presence_range;
    uint32_t channel_mask;
} temp_settings;

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

 void settings_slider_event_cb(lv_event_t * e) {
  lv_obj_t * slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    
    // Identify which slider was moved and update temporary values
    if(slider == g_screens[SCREEN_SETTINGS].settings.high_temp_slider) {
        g_screens[SCREEN_SETTINGS].settings.temp_values.high_temp = value;
        char buf[32];
        snprintf(buf, sizeof(buf), "High: %d°C", value);
        lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.high_temp_label, buf);
        ESP_LOGI("SLIDER", "High temp slider: %d", value);
    }
    else if(slider == g_screens[SCREEN_SETTINGS].settings.low_temp_slider) {
        g_screens[SCREEN_SETTINGS].settings.temp_values.low_temp = value;
        char buf[32];
        snprintf(buf, sizeof(buf), "Low: %d°C", value);
        lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.low_temp_label, buf);
        ESP_LOGI("SLIDER", "Low temp slider: %d", value);
    }
    else if(slider == g_screens[SCREEN_SETTINGS].settings.presence_range_slider) {
        g_screens[SCREEN_SETTINGS].settings.temp_values.presence_range = value * 100;
        char buf[32];
        snprintf(buf, sizeof(buf), "Range: %.1fm", value/10.0f);
        lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.range_label, buf);
        ESP_LOGI("SLIDER", "Range slider: %d", value);
    }
    else if(slider == g_screens[SCREEN_SETTINGS].settings.channel_slider) {
        g_screens[SCREEN_SETTINGS].settings.temp_values.channel_mask = (1UL << value);
        char buf[32];
        snprintf(buf, sizeof(buf), "Channel: %d", value);
        lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.channel_label, buf);
        ESP_LOGI("SLIDER", "Channel slider: %d", value);
    }

    // // Trigger UI update to reflect changes
    // ui_event_t event = {
    //     .target_screen = SCREEN_SETTINGS,
    //     .message = ""
    // };
    // xQueueSend(ui_event_queue, &event, 0);
}

static void settings_save_btn_cb(lv_event_t * e) {
    // Save temporary settings to globals and NVS
    g_target_high_temp = temp_settings.high_temp;
    g_target_low_temp = temp_settings.low_temp;
    g_range_limit = temp_settings.presence_range;
    g_channel_mask = temp_settings.channel_mask;
    
    save_settings(temp_settings.channel_mask, 
                 temp_settings.high_temp, 
                 temp_settings.low_temp,
                 temp_settings.presence_range);
                 
    // Call the registered callback if exists
    if (settings_callback) {
        settings_callback(settings_callback_param);
    }
    
    ui_switch_screen(SCREEN_MAIN);
}

static void settings_cancel_btn_cb(lv_event_t * e) {
    // Discard temporary changes and return to main screen
    ui_switch_screen(SCREEN_MAIN);
}

// Add this function to register the callbacks with the UI elements
void settings_init_callbacks(void) {
    // Initialize temp settings with current values
    temp_settings.high_temp = g_target_high_temp;
    temp_settings.low_temp = g_target_low_temp;
    temp_settings.presence_range = g_range_limit;
    temp_settings.channel_mask = g_channel_mask;
    
    // Register callbacks
    lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.high_temp_slider, 
                        settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.low_temp_slider, 
                        settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.presence_range_slider, 
                        settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.channel_slider, 
                        settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.save_btn, 
                        settings_save_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.cancel_btn, 
                        settings_cancel_btn_cb, LV_EVENT_CLICKED, NULL);
}