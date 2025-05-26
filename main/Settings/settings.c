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
#include "Zigbee/zigbee.h"

#define TAG "SETTINGS"

static settings_callback_t settings_callback = NULL;
static SemaphoreHandle_t settings_callback_param = NULL;

static struct
{
    uint8_t high_temp;
    uint8_t low_temp;
    uint8_t presence_range;
    uint8_t room;
} temp_settings;

void settings_complete_cb(SemaphoreHandle_t semaphore)
{
    // check if we have devices
    if (stored_device_count == 0)
    {
        ui_switch_screen(SCREEN_BOOT); // Switch back to boot screen
        // Return to boot screen
        ui_event_t event = {
            .target_screen = SCREEN_BOOT,
            .message = "Settings saved, starting Zigbee network..."};
        xQueueSend(ui_event_queue, &event, portMAX_DELAY);
        vTaskDelay(SHORT_DELAY); // Give time for message to be displayed
        ESP_LOGI("Settings_complete_cb", "Settings saved, starting Zigbee network...");
        // This is called when the user finishes setting up or cancels settings
        xSemaphoreGive(semaphore);
    }
    else
    {
        ui_switch_screen(SCREEN_BOOT); // Switch back to boot screen
        // Return to boot screen
        ui_event_t event = {
            .target_screen = SCREEN_BOOT,
            .message = "Settings saved, returning to main screen"};
        xQueueSend(ui_event_queue, &event, portMAX_DELAY);
        vTaskDelay(SHORT_DELAY);       // Give time for message to be displayed
        ui_switch_screen(SCREEN_MAIN); // Switch back to boot screen
    }
}
bool save_settings(uint16_t room, uint16_t target_temp, uint16_t min_temp, uint16_t presence_range)
{
    ESP_LOGI(TAG, "Saving settings: room: 0x%d, Target Temp: %d, Min Temp: %d, Range: %d",
             room, target_temp, min_temp, presence_range);
    esp_err_t err;
    nvs_handle_t nvs_handle;
    // save into globals
    g_room = room;
    g_target_high_temp = target_temp;
    g_target_low_temp = min_temp;
    g_range_limit = presence_range; // Convert to cm for storage

    err = nvs_open("zigbee", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return false;
    }

    // Save channel mask
    err = nvs_set_u32(nvs_handle, "room", room);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error saving room number: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    // Save other settings
    err = nvs_set_u16(nvs_handle, "target_temp", target_temp);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error saving target temp: %s", esp_err_to_name(err));
    }

    err = nvs_set_u16(nvs_handle, "min_temp", min_temp);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error saving min temp: %s", esp_err_to_name(err));
    }

    err = nvs_set_u16(nvs_handle, "range_limit", presence_range);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error saving max temp: %s", esp_err_to_name(err));
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error committing NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    nvs_close(nvs_handle);

    // If a callback is registered for settings completion, call it
    if (settings_callback && settings_callback_param)
    {
        settings_callback(settings_callback_param);
    }

    return true;
}

void settings_register_callback(settings_callback_t callback, SemaphoreHandle_t param)
{
    settings_callback = callback;
    settings_callback_param = param;
}

void settings_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);

    // Identify which slider was moved and update temporary values
    if (slider == g_screens[SCREEN_SETTINGS].settings.high_temp_slider)
    {
        g_screens[SCREEN_SETTINGS].settings.temp_values.high_temp = value;
        char buf[32];
        snprintf(buf, sizeof(buf), "High: %d°C", value);
        lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.high_temp_label, buf);
        ESP_LOGI("SLIDER", "High temp slider: %d", value);
        temp_settings.high_temp = value; // Update temporary settings
    }
    else if (slider == g_screens[SCREEN_SETTINGS].settings.low_temp_slider)
    {
        g_screens[SCREEN_SETTINGS].settings.temp_values.low_temp = value;
        char buf[32];
        snprintf(buf, sizeof(buf), "Low: %d°C", value);
        lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.low_temp_label, buf);
        ESP_LOGI("SLIDER", "Low temp slider: %d", value);
        temp_settings.low_temp = value; // Update temporary settings
    }
    else if (slider == g_screens[SCREEN_SETTINGS].settings.presence_range_slider)
    {
        g_screens[SCREEN_SETTINGS].settings.temp_values.presence_range = value * 100;
        char buf[32];
        snprintf(buf, sizeof(buf), "Range: %dm", value);
        lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.range_label, buf);
        ESP_LOGI("SLIDER", "Range slider: %d", value);
        temp_settings.presence_range = value; // Update temporary settings
    }
    else if (slider == g_screens[SCREEN_SETTINGS].settings.room_slider)
    {
        g_screens[SCREEN_SETTINGS].settings.temp_values.room = value;
        char buf[32];
        snprintf(buf, sizeof(buf), "Room: %d", value);
        lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.room_label, buf);
        ESP_LOGI("SLIDER", "Room slider: %d", value);
        temp_settings.room = value; // Update temporary settings
    }
}

void settings_save_btn_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "save_btn_cb high temp: %d, low temp: %d, range: %d, room: %d",
             temp_settings.high_temp, temp_settings.low_temp,
             temp_settings.presence_range, temp_settings.room);
    // Save temporary settings to globals and NVS
    g_target_high_temp = temp_settings.high_temp;
    g_target_low_temp = temp_settings.low_temp;
    g_range_limit = temp_settings.presence_range;
    g_room = temp_settings.room;

    save_settings(temp_settings.room,
                  temp_settings.high_temp,
                  temp_settings.low_temp,
                  temp_settings.presence_range);

    ui_switch_screen(SCREEN_BOOT);
}

void settings_cancel_btn_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Settings changes discarded, returning to main screen");
    // Discard temporary changes and return to main screen
    if (settings_callback)
    {
        settings_callback(settings_callback_param);
    }
    ui_switch_screen(SCREEN_BOOT);
}

// Add this function to register the callbacks with the UI elements
void settings_init_callbacks(void)
{
    // Initialize temp settings with current values
    temp_settings.high_temp = g_target_high_temp;
    temp_settings.low_temp = g_target_low_temp;
    temp_settings.presence_range = g_range_limit;
    temp_settings.room = g_room;

    // Register callbacks
    lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.high_temp_slider,
                        settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.low_temp_slider,
                        settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.presence_range_slider,
                        settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.room_slider,
                        settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.save_btn,
                        settings_save_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.cancel_btn,
                        settings_cancel_btn_cb, LV_EVENT_CLICKED, NULL);
}

esp_err_t load_settings_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("zigbee", NVS_READONLY, &nvs_handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Load channel mask
    uint16_t room = 1; // Default room number
    err = nvs_get_u16(nvs_handle, "room", &room);
    if (err == ESP_OK)
    {
        g_room = room;
    }

    // Load other settings
    uint16_t temp;
    if (nvs_get_u16(nvs_handle, "target_temp", &temp) == ESP_OK)
    {
        g_target_high_temp = temp;
    }
    if (nvs_get_u16(nvs_handle, "min_temp", &temp) == ESP_OK)
    {
        g_target_low_temp = temp;
    }
    if (nvs_get_u16(nvs_handle, "range_limit", &temp) == ESP_OK)
    {
        g_range_limit = temp / 100.0f; // Convert back to meters
    }

    nvs_close(nvs_handle);

    return ESP_OK;
}