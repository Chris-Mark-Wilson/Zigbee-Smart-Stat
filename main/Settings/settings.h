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

static float g_range_limit = 30.0f;       // Initial range limit in centimeters (2-7m)
static uint8_t g_target_high_temp = 21;  // Default high temp (17-21)
static uint8_t g_target_low_temp = 16;   // Default low temp (13-16)


void update_range_limit(float new_limit);

#endif // SETTINGS_H