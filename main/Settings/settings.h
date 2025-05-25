#ifndef SETTINGS_H
#define SETTINGS_H


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


// control logic parameters
#define MIN_SAFETY_TEMP 16.0      // Safety temperature threshold (never go below this)
#define COMFORT_TEMP 21.0         // Comfort temperature when presence detected
#define PRESENCE_TIMEOUT_MS 10000 // 30 seconds threshold for presence detection
// TRV control settings
#define TRV_TEMP_MAX 30 // Maximum temperature (TRV ON)
#define TRV_TEMP_MIN 5  // Minimum temperature (TRV OFF)

typedef void (*settings_callback_t)(SemaphoreHandle_t param);
static settings_callback_t settings_callback;
static SemaphoreHandle_t settings_callback_param;

static float g_range_limit = 30.0f;       // Initial range limit in centimeters (2-7m)
static uint8_t g_target_high_temp = 21;  // Default high temp (17-21)
static uint8_t g_target_low_temp = 16;   // Default low temp (13-16)
static uint32_t g_channel_mask = 0x00002000; // Default channel mask (13)


void update_range_limit(float new_limit);
void settings_complete_cb(SemaphoreHandle_t semaphore);
bool save_settings(uint32_t channel_mask, uint16_t target_temp, uint16_t min_temp, uint16_t max_temp);
void settings_register_callback(settings_callback_t callback, SemaphoreHandle_t param);

#endif // SETTINGS_H