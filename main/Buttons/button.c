#include "button.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "helpers.h"
#include "zigbee.h"
#include "ui_events.h"

static const char *TAG = "BUTTON ";
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
    xTaskCreate(button_task, "button_task", 4096, NULL, 10, NULL);

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
// Callback function for button press
void button_pressed_cb(button_event_t event)
{

    switch (event)
    {
    case BUTTON_PRESSED:
    {
        ESP_LOGI(TAG, "Button pressed - waiting for release");
        // address of the window sensor and endpoint

        uint16_t addr = stored_devices[0].short_addr;  // Replace with actual address
        uint8_t endpoint = stored_devices[0].endpoint; // Replace with actual endpoint
        read_window_sensor_status(addr, endpoint);

        break;
    }

    case BUTTON_RELEASED:
    {
        if (current_screen == SCREEN_BOOT && network_open)
        {
            ESP_LOGI(TAG, "Closing network on button press");
            close_network();
            ui_switch_screen(SCREEN_MAIN);
        }
        else if (current_screen == SCREEN_MAIN)
        {
            if (stored_device_count > 0)
            {
                show_stored_devices();
                // Toggle between min and max temperature and corresponding mode
                g_trv_state = !g_trv_state;
                g_target_temp = g_trv_state ? g_max_temp : g_min_temp;
                if (g_trv_state)
                {
                    turn_trvs_on();
                }
                else
                {
                    turn_trvs_off();
                }

                // // Also display the network key for debugging
                display_network_key();
            }
            else
            {
                ESP_LOGW(TAG, "No devices stored - cannot send commands");
                // Display network key even if no devices are paired
                display_network_key();
            }
        }
        break;
    }
    case BUTTON_LONG_PRESS:
    {
        ESP_LOGI(TAG, "Long press detected - sending leave request to all devices");

        
   ui_event_t ui_event = {
            .target_screen = current_screen,
            .message = "Resetting device..."};
        xQueueSend(ui_event_queue, &ui_event, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "Unpairing all devices");
        strcpy(ui_event.message , "Unpairing devices...");
        xQueueSend(ui_event_queue, &ui_event, 0);
        reset_device();
        vTaskDelay(pdMS_TO_TICKS(1000));

    
        break;
    }
    }
}