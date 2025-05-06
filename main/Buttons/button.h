#ifndef BUTTON_H
#define BUTTON_H

#include "esp_err.h"
#include "driver/gpio.h"

// Define the GPIO pin for the button
#define NETWORK_CONTROL_BTN_PIN GPIO_NUM_9  // Change this to your actual GPIO pin
#define DEBOUNCE_TIME_MS 100   // Debounce time in milliseconds

// Function declarations
esp_err_t button_init(void);
bool button_is_pressed(void);

// Callback type for button press
typedef void (*button_callback_t)(void);

// Register callback for button press
void button_register_callback(button_callback_t callback);

#endif // BUTTON_H