#ifndef BUTTON_H
#define BUTTON_H

#include "esp_err.h"
#include "driver/gpio.h"

#define NETWORK_CONTROL_BTN_PIN GPIO_NUM_9
#define DEBOUNCE_TIME_MS 50  // Reduced debounce time
#define LONG_PRESS_THRESHOLD_MS 2000  // Reduced to 2 seconds

typedef enum {
    BUTTON_PRESSED,
    BUTTON_RELEASED,
    BUTTON_LONG_PRESS
} button_event_t;

typedef void (*button_callback_t)(button_event_t event);

esp_err_t button_init(void);
void button_register_callback(button_callback_t callback);

#endif