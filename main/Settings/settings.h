#ifndef SETTINGS_H
#define SETTINGS_H


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lvgl.h" 


// control logic parameters
#define MIN_SAFETY_TEMP 16.0      // Safety temperature threshold (never go below this)
#define COMFORT_TEMP 21.0         // Comfort temperature when presence detected
#define PRESENCE_TIMEOUT_MS 10000 // 30 seconds threshold for presence detection
// TRV control settings
#define TRV_TEMP_MAX 30 // Maximum temperature (TRV ON)
#define TRV_TEMP_MIN 5  // Minimum temperature (TRV OFF)
#define MAX_ROOMS 10 //max rooms in house, used for settings slider max value

typedef void (*settings_callback_t)(SemaphoreHandle_t param);
extern settings_callback_t settings_callback;
extern SemaphoreHandle_t settings_callback_param;

extern uint8_t g_range_limit;       // Range limit in meters
extern uint8_t g_target_high_temp;  // High temp (17-21)
extern uint8_t g_target_low_temp;   // Low temp (13-16)
extern uint8_t g_room;              // Room number (1-MAX_ROOMS)

void settings_complete_cb(SemaphoreHandle_t semaphore);
bool save_settings(uint16_t room, uint16_t target_temp, uint16_t min_temp, uint16_t presence_range);
void settings_register_callback(settings_callback_t callback, SemaphoreHandle_t param);
void settings_init_callbacks(void);
void settings_slider_event_cb(lv_event_t *e);
void settings_cancel_btn_cb(lv_event_t *e);
void settings_save_btn_cb(lv_event_t *e);
esp_err_t load_settings_from_nvs(void);

#endif // SETTINGS_H