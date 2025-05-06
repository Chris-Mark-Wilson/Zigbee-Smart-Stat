#include "button.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

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
    
    while(1) {
        if(xQueueReceive(button_evt_queue, &gpio_num, portMAX_DELAY)) {
            TickType_t current_time = xTaskGetTickCount();
            
            // Debounce check
            if((current_time - last_press_time) >= pdMS_TO_TICKS(DEBOUNCE_TIME_MS)) {
                // Additional verification of button state
                if(gpio_get_level(gpio_num) == 0) { // Active LOW with pull-up
                    ESP_LOGI(TAG, "Button pressed (GPIO %d)", gpio_num);
                    if(button_callback) {
                        button_callback();
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
        .intr_type = GPIO_INTR_NEGEDGE,      // Interrupt on falling edge
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

void button_register_callback(button_callback_t callback)
{
    button_callback = callback;
}