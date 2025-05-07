#include "button.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"

static const char *TAG = "BUTTON";
static button_callback_t button_callback = NULL;
static bool is_isr_installed = false;

// Use a FreeRTOS queue for button events
static QueueHandle_t button_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(button_evt_queue, &gpio_num, NULL);
}

// Button processing task
static void button_task(void *arg)
{
    uint32_t gpio_num;
    TickType_t last_press_time = 0;
    int64_t press_start = 0;
    bool is_pressed = false;
    
    while(1) {
        if(xQueueReceive(button_evt_queue, &gpio_num, portMAX_DELAY)) {
            TickType_t current_time = xTaskGetTickCount();
            
            // Debounce check - reduced to 50ms for better responsiveness
            if((current_time - last_press_time) >= pdMS_TO_TICKS(50)) {
                int level = gpio_get_level(gpio_num);
                
                if (level == 0 && !is_pressed) { // Button pressed
                    is_pressed = true;
                    press_start = esp_timer_get_time() / 1000;
                    ESP_LOGI(TAG, "Button pressed");
                }
                else if (level == 1 && is_pressed) { // Button released
                    is_pressed = false;
                    int64_t press_duration = (esp_timer_get_time() / 1000) - press_start;
                    
                    if (press_duration >= LONG_PRESS_THRESHOLD_MS) {
                        ESP_LOGI(TAG, "Long press detected (%lld ms)", press_duration);
                        if (button_callback) button_callback(BUTTON_LONG_PRESS);
                    } else {
                        ESP_LOGI(TAG, "Short press detected (%lld ms)", press_duration);
                        if (button_callback) button_callback(BUTTON_RELEASED);
                    }
                }
                last_press_time = current_time;
            }
        }
    }
}

esp_err_t button_init(void)
{
    // Create button event queue before installing ISR
    button_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if (!button_evt_queue) {
        ESP_LOGE(TAG, "Failed to create button event queue");
        return ESP_FAIL;
    }

    // Configure button GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << NETWORK_CONTROL_BTN_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,    // Enable pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,      // Changed to detect both edges
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error configuring GPIO: %d", ret);
        return ret;
    }

    // Install ISR service if not already installed
    if (!is_isr_installed) {
        ret = gpio_install_isr_service(0);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Error installing GPIO ISR service: %d", ret);
            return ret;
        }
        is_isr_installed = true;
    }

    // Add ISR handler
    ret = gpio_isr_handler_add(NETWORK_CONTROL_BTN_PIN, gpio_isr_handler, (void*) NETWORK_CONTROL_BTN_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error adding ISR handler: %d", ret);
        return ret;
    }

    // Create button processing task
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "Button initialized on GPIO %d", NETWORK_CONTROL_BTN_PIN);
    return ESP_OK;
}

bool button_is_pressed(void)
{
    return (gpio_get_level(NETWORK_CONTROL_BTN_PIN) == 0);
}
//this function is in main.c
// // Callback function for button press
// static void button_pressed_cb(void)
// {
//     ESP_LOGI(TAG, "Button pressed!");
//     // We'll add network control logic here later
// }

void button_register_callback(button_callback_t callback)
{
    button_callback = callback;
}