
#include "switch_driver.h"
#include "dht.h"
#include "string.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// #include "ha/esp_zigbee_ha_standard.h"

//lcd headers
#include "ST7789.h"

//zigbee headers

#include "Zigbee/zigbee.h"
#include "Buttons/button.h"
#include "LVGL_UI/ui_screens.h"
#include "LVGL_UI/ui_events.h"
//sensors headers
#include "Sensors/sensors.h"
//settings headers
#include "Settings/settings.h"


// Global state tracking variables
static int64_t g_last_presence_time = 0; // Last time presence was detected
static bool g_is_window_open=false; // Global window state
static bool g_presence_detected = false; // Global presence state
// static uint8_t g_current_range = 0;      // current range read from hmmd sensor


#if defined ZB_ED_ROLE
#error Define ZB_COORDINATOR_ROLE in idf.py menuconfig to compile source code.
#endif

// Create queue handle for UI events
QueueHandle_t ui_event_queue;

//this exact function name MUST remain in this file
// it is used by the zigbee library to call the signal handler
//and it expects to find it in here or it just wont fucking work
//its so i can put all my zigbee code in one file and not have to worry about it
// being in the right place
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    zigbee_signal_handler(signal_struct);
}

//handle window sensor state change
static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    #define TAG "ZB_ATTR_HANDLER"
    ESP_LOGI(TAG, "Zigbee attribute handler called with message: %p", message);
    esp_err_t ret = ESP_OK;
    bool window_state = 0;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);
    if (message->info.dst_endpoint == 1){ // Window sensor endpoint{
        if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
                window_state = message->attribute.data.value ? *(bool *)message->attribute.data.value : window_state;
                ESP_LOGI(TAG, "Light sets to %s", window_state ? "On" : "Off");
                // light_driver_set_power(light_state);
            }
        }
    }
    return ret;
}
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    #define TAG "ZB_ACTION_HANDLER"
    ESP_LOGI(TAG, "Zigbee action handler called with callback ID: %d", callback_id);
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

// TODO implement when everything is bound and getting data
static void evaluate_control_logic(void){}

static void lvgl_task(void *pvParameters)
{
    while (1)
    {
        lv_task_handler();  // Keep this as it's needed for LVGL core functionality
        vTaskDelay(pdMS_TO_TICKS(10));  // Reduced delay for smoother updates
    }
}

static void dht_sensor_task(void *pvParameters)
{

    #define TAG "DHT_SENSOR"
    // Initialize dht sensor
    esp_err_t dht_ret = dht_sensor_init();
   
    if (dht_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize DHT sensor");
        return;
    }

    // Initialize timer for control logic
    g_last_presence_time = esp_timer_get_time() / 1000; // Current time in ms


    ESP_LOGI(TAG, "temp and humidity monitoring task started");
    ESP_LOGI(TAG, "Control settings: Safety temp: %.1f°C, Comfort temp: %.1f°C, Presence timeout: %d sec",
             MIN_SAFETY_TEMP, COMFORT_TEMP, PRESENCE_TIMEOUT_MS / 1000);

 
    // Main task loop

    while (1)
    {
        // Read temperature sensor at normal interval (less frequent)
        static int temp_counter = 0;
        if (g_dht_initialised && temp_counter++ >= (DHT_READ_INTERVAL_MS / PRESENCE_SAMPLE_INTERVAL_MS))
        {
            read_dht_sensor();
            temp_counter = 0;

        }
  
        // Run control logic at normal interval
        if (temp_counter == 0)
        {
            evaluate_control_logic();
            ESP_LOGI(TAG, "DHT Readings - Temperature: %.1f°C, Humidity: %.1f%%", g_temperature, g_humidity);
            ui_event_t event = {
                .target_screen = SCREEN_MAIN,
                .message = ""
            };
            xQueueSend(ui_event_queue, &event, 0);
        }

        // Shorter delay for presence sampling
        vTaskDelay(pdMS_TO_TICKS(PRESENCE_SAMPLE_INTERVAL_MS));
    }
}

static void zigbee_task(void *pvParameters)
{
    // Initialize Zigbee stack as Coordinator
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    // Set channel mask
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);

    // Start Zigbee stack with autostart
    ESP_ERROR_CHECK(esp_zb_start(true));  // Changed to true for autostart

    // Enter Zigbee main loop
    esp_zb_stack_main_loop();
}



static void hmmd_read_task(void *arg)
{
    if(!g_hmmd_initialised)
    {
        ESP_LOGE(TAG, "HMMD sensor not initialized");
        return;
    }
    uint8_t data[128];
    char *range_str;
    
    ESP_LOGI(TAG, "HMMD task started with initial range limit: %.1f cm", g_range_limit);
    
    while (1) {
        int len = uart_read_bytes(HMMD_UART_NUM, data, sizeof(data) - 1, pdMS_TO_TICKS(100));
        
        if (len > 0) {
            data[len] = '\0';
            
            // Look for "Range " instead of "range:"
            range_str = strstr((char *)data, "Range ");
            if (range_str) {
                float range_cm;
                if (sscanf(range_str, "Range %f", &range_cm) == 1) {
                    g_current_range = range_cm;  // Update the global range variable defined in ui_screens.h
                    // Update presence state
                 
                    g_presence_detected = (range_cm < g_range_limit);

                    // Always trigger UI update when we get valid range data
                    ui_event_t event = {
                        .target_screen = SCREEN_MAIN,
                        .message = ""
                    };
                    xQueueSend(ui_event_queue, &event, 0);
                } else {
                    ESP_LOGW(TAG, "Failed to parse range value from: %s", range_str);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}




// Callback function for button press
static void button_pressed_cb(button_event_t event)
{
    switch (event) {
        case BUTTON_PRESSED:
        
            ESP_LOGI(TAG, "Button pressed - waiting for release");
            break;
        case BUTTON_RELEASED:
            if (network_open) {
                ESP_LOGI(TAG, "Closing network on button press");
                close_network();
                 // Toggle screens when network is closed
                 screen_id_t next_screen = (current_screen + 1) % SCREEN_COUNT;
                 if (next_screen != SCREEN_BOOT) { // Skip boot screen during normal operation
                     ui_switch_screen(next_screen);
            } else {
                // Toggle screens when network is closed
                screen_id_t next_screen = (current_screen + 1) % SCREEN_COUNT;
                if (next_screen != SCREEN_BOOT) { // Skip boot screen during normal operation
                    ui_switch_screen(next_screen);
                }
            }
            break;
            case BUTTON_LONG_PRESS: {
                ESP_LOGI(TAG, "Long press detected - leaving network and clearing NVS");
                // First leave the network
                esp_zb_zdo_mgmt_leave_req_param_t leave_req = {
                    .dst_nwk_addr = 0x0000,      // Coordinator address
                    .rejoin = 0,                 // No rejoin
                    .remove_children = 1         // Remove children
                };
                esp_zb_zdo_device_leave_req(&leave_req, NULL, NULL);
                vTaskDelay(pdMS_TO_TICKS(1000));   // Give time for leave to process
                // Then clear NVS
                clear_all_nvs();
                esp_restart();
                break;
            }
            
        
    }
}
}

static void ui_update_task(void *pvParameters) {
    ui_event_t event;
    
    while (1) {
        if (xQueueReceive(ui_event_queue, &event, portMAX_DELAY)) {
            // Handle UI update in LVGL context
            switch (event.target_screen) {
                case SCREEN_BOOT:
                    ui_update_boot_status(event.message);
                    break;
                    case SCREEN_MAIN:
                    ui_update_main_screen(
                        g_temperature > 0 ? g_temperature : 0.0f,
                        g_humidity > 0 ? g_humidity : 0.0f,
                        g_presence_detected,
                        g_is_window_open
                    );
                    break;
                    
                case SCREEN_SETTINGS:
                    ui_update_settings(
                        g_target_high_temp,
                        g_target_low_temp,
                        g_current_range
                    );
                    break;

                case SCREEN_COUNT:
                // This is just a count value, shouldn't be used as a screen
                ESP_LOGW(TAG, "Invalid screen value SCREEN_COUNT received");
                break;
            
            default:
                ESP_LOGW(TAG, "Unknown screen value: %d", event.target_screen);
                break;
                // Add other screen updates as needed
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Give other tasks time
    }
}

void app_main(void)
{
    // Initialize button
    ESP_ERROR_CHECK(button_init());
    button_register_callback(button_pressed_cb);

    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    LCD_Init();   
    LVGL_Init(); 
    esp_err_t ret = hmmd_uart_init();
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize HMMD UART");
        return;
    } else {
        ESP_LOGI(TAG, "HMMD UART initialized successfully");
        g_hmmd_initialised = true;
    }

    // Create UI event queue first
    ui_event_queue = xQueueCreate(10, sizeof(ui_event_t));
    if (ui_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create UI event queue");
        return;
    }

    // Initialize UI screens
    ui_init_screens();

    // Create tasks after queue is initialized
    xTaskCreate(lvgl_task, "lvgl_handler", 4096, NULL, 6, NULL);
    xTaskCreate(ui_update_task, "ui_update", 4096, NULL, 5, NULL);
    xTaskCreate(zigbee_task, "Zigbee_main", 4096, NULL, 5, NULL);
    xTaskCreate(hmmd_read_task, "HMMD_read", 2048, NULL, 4, NULL);
    xTaskCreate(dht_sensor_task, "DHT_sensor", 2048, NULL, 4, NULL);
    // xTaskCreate(dht_sensor_task, "DHT_sensor", 2048, NULL, 4, NULL);
}
