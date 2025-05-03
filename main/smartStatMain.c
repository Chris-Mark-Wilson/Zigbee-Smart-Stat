/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Zigbee HA_thermostat Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_zb_thermostat.h"
#include "switch_driver.h"
#include "dht.h"
#include "string.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "esp_timer.h"
//lcd headers
#include "ST7789.h"
#include "LVGL_smart_stat_ui.h"
//zigbee headers
#include "zigbee.h"
#include "sensors.h"




// control logic parameters
#define MIN_SAFETY_TEMP 16.0      // Safety temperature threshold (never go below this)
#define COMFORT_TEMP 21.0         // Comfort temperature when presence detected
#define PRESENCE_TIMEOUT_MS 5000 // 30 seconds threshold for presence detection
// TRV control settings
#define TRV_TEMP_MAX 30 // Maximum temperature (TRV ON)
#define TRV_TEMP_MIN 5  // Minimum temperature (TRV OFF)


static bool g_presence_detected = false; // Global presence state

// Global temperature and humidity storage
static float g_temperature = 0;
static float g_humidity = 0;

// Global state tracking variables
static int64_t g_last_presence_time = 0; // Last time presence was detected
static bool g_trv_is_on = false;         // Current TRV state
static uint8_t g_presence_counter = 0;   // Counter for debouncing
static bool g_last_raw_presence = false; // Last raw reading

#if defined ZB_ED_ROLE
#error Define ZB_COORDINATOR_ROLE in idf.py menuconfig to compile thermostat source code.
#endif



// read the temp sensor function
static esp_err_t read_dht_sensor(void)
{
    if (!g_dht_initialized)
    {
        ESP_LOGE(TAG, "DHT sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    float temperature = 0;
    float humidity = 0;

    // Use dht_read_float_data directly from the library
    esp_err_t ret = dht_read_float_data(DHT_TYPE, DHT_GPIO_PIN, &humidity, &temperature);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read data from DHT sensor: %s", esp_err_to_name(ret));
        return ret;
    }

    g_temperature = temperature;
    g_humidity = humidity;

    ESP_LOGI(TAG, "DHT22 Readings - Temperature: %.1f°C, Humidity: %.1f%%", temperature, humidity);
    return ESP_OK;
}

// read the presence sensor function
static esp_err_t read_rcwl_sensor(void)
{
    if (!g_rcwl_initialized)
    {
        ESP_LOGE(TAG, "RCWL sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Read the GPIO pin (raw reading)
    int level = gpio_get_level(RCWL_GPIO_PIN);
    bool current_raw_presence = (level == 1);

    // Debouncing logic
    if (current_raw_presence != g_last_raw_presence)
    {
        // Reset counter on state change
        g_presence_counter = 1;
        g_last_raw_presence = current_raw_presence;
    }
    else
    {
        // Increment counter for consistent readings
        if (g_presence_counter < PRESENCE_DEBOUNCE_COUNT)
        {
            g_presence_counter++;
        }
        else if (g_presence_counter == PRESENCE_DEBOUNCE_COUNT)
        {
            // We've reached the threshold, update the actual presence state
            if (g_presence_detected != current_raw_presence)
            {
                g_presence_detected = current_raw_presence;
                ESP_LOGI(TAG, "RCWL Presence state changed: %s",
                         g_presence_detected ? "DETECTED" : "NOT DETECTED");
            }
        }
    }

    return ESP_OK;
}

// Function to set TRV target temperature
static void set_trv_temperature(uint8_t target_temp)
{
    int16_t target_temp_value = target_temp * 100; // Convert to centi-degrees

    // Detailed connection status check with address info
    if (temp_sensor.short_addr == 0)
    {
        ESP_LOGW(TAG, "[TRV] No valid TRV connection yet, skipping temperature command (target: %d°C)", target_temp);
        return;
    }

    ESP_LOGI(TAG, "[TRV] Sending temperature command to TRV (addr: 0x%04x, ep: %d) → %d°C",
             temp_sensor.short_addr, temp_sensor.endpoint, target_temp);

    esp_zb_zcl_write_attr_cmd_t write_req = {0};
    write_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    write_req.zcl_basic_cmd.src_endpoint = HA_THERMOSTAT_ENDPOINT;
    write_req.zcl_basic_cmd.dst_endpoint = temp_sensor.endpoint;
    write_req.zcl_basic_cmd.dst_addr_u.addr_short = temp_sensor.short_addr;
    write_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT;

    esp_zb_zcl_attribute_t attr = {
        .id = ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID,
        .data.type = ESP_ZB_ZCL_ATTR_TYPE_S16,
        .data.value = &target_temp_value};

    write_req.attr_field = &attr;
    write_req.attr_number = 1;

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_err_t err = esp_zb_zcl_write_attr_cmd_req(&write_req);
    esp_zb_lock_release();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "[TRV] Failed to send temperature command: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "[TRV] Temperature command sent successfully");
    }
}
// Function to turn TRV on
static void turn_trv_on(const char *reason)
{
    if (!g_trv_is_on)
    {
        ESP_LOGI(TAG, "Turning TRV ON - Reason: %s", reason);
        set_trv_temperature(TRV_TEMP_MAX);
        g_trv_is_on = true;
    }
}

// Function to turn TRV off
static void turn_trv_off(const char *reason)
{
    if (g_trv_is_on)
    {
        // Safety check - don't turn off if below minimum safety temperature
        if (g_temperature < MIN_SAFETY_TEMP)
        {
            ESP_LOGI(TAG, "Refusing to turn TRV OFF - Temperature %.1f°C below safety threshold %.1f°C",
                     g_temperature, MIN_SAFETY_TEMP);
            return;
        }

        ESP_LOGI(TAG, "Turning TRV OFF - Reason: %s", reason);
        set_trv_temperature(TRV_TEMP_MIN);
        g_trv_is_on = false;
    }
}
// Function to check if TRV is connected and available
static bool is_trv_connected(void)
{
    bool connected = (temp_sensor.short_addr != 0);
    return connected;
}

// Control logic function
static void evaluate_control_logic(void)
{
    int64_t current_time = esp_timer_get_time() / 1000; // Current time in milliseconds
    int64_t time_since_last_presence = current_time - g_last_presence_time;

    // Update presence timestamp if currently detected
    if (g_presence_detected)
    {
        g_last_presence_time = current_time;
    }

    // Check TRV connection status
    bool trv_connected = is_trv_connected();

    // Log current system state
    ESP_LOGI(TAG, "Status: Temp=%.1f°C | Presence: %s | TRV: %s | TRV Status: %s",
             g_temperature,
             g_presence_detected ? "YES" : "NO",
             trv_connected ? "CONNECTED" : "NOT CONNECTED",
             g_trv_is_on ? "ON" : "OFF");

    // Don't attempt control logic if TRV not connected
    if (!trv_connected)
    {
        ESP_LOGW(TAG, "[TRV] Cannot control TRV - not connected");
        return;
    }

    // === Control logic based on the requirements ===

    // Safety threshold - don't let temperature drop too low
    if (g_temperature < MIN_SAFETY_TEMP)
    {
        turn_trv_on("Temperature below minimum safety threshold");
        return;
    }

    // Turn off TRV if temperature is above comfort level (even if presence is detected)
    if (g_temperature >= COMFORT_TEMP)
    {
        turn_trv_off("Temperature above or at comfort threshold");
        return;
    }

    // Comfort control when presence detected and temperature below comfort level
    if (g_presence_detected && g_temperature < COMFORT_TEMP)
    {
        turn_trv_on("Presence detected and temperature below comfort threshold");
        return;
    }

    // Turn off if no presence for specified timeout
    if (time_since_last_presence > PRESENCE_TIMEOUT_MS)
    {
        turn_trv_off("No presence detected for timeout period");
        return;
    }
} // Updated sensor task with control logic

static void dht_sensor_task(void *pvParameters)
{
    // Initialize both sensors
    esp_err_t dht_ret = dht_sensor_init();
    esp_err_t rcwl_ret = rcwl_sensor_init();

    // Initialize timer for control logic
    g_last_presence_time = esp_timer_get_time() / 1000; // Current time in ms

    if (dht_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize DHT sensor");
    }

    if (rcwl_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize RCWL sensor");
    }

    if (dht_ret != ESP_OK && rcwl_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "All sensors failed to initialize, ending task");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Sensor monitoring task started");
    ESP_LOGI(TAG, "Control settings: Safety temp: %.1f°C, Comfort temp: %.1f°C, Presence timeout: %d sec",
             MIN_SAFETY_TEMP, COMFORT_TEMP, PRESENCE_TIMEOUT_MS / 1000);

    // Update reading interval to 2 seconds
    const int READ_INTERVAL_MS = 2000;

    // Main task loop

    while (1)
    {
        // Read temperature sensor at normal interval (less frequent)
        static int temp_counter = 0;
        if (g_dht_initialized && temp_counter++ >= (READ_INTERVAL_MS / PRESENCE_SAMPLE_INTERVAL_MS))
        {
            read_dht_sensor();
            temp_counter = 0;

            // Combined sensor status log after temperature reading
            if (g_dht_initialized && g_rcwl_initialized)
            {
                ESP_LOGI(TAG, "Status: Temp=%.1f°C, Humidity: %.1f%%, Presence: %s",
                         g_temperature, g_humidity, g_presence_detected ? "DETECTED" : "NOT DETECTED");
            }
        }

        // Read presence sensor more frequently for debouncing
        if (g_rcwl_initialized)
        {
            read_rcwl_sensor();
        }

        // Run control logic at normal interval
        if (temp_counter == 0)
        {
            evaluate_control_logic();
        }

        // Shorter delay for presence sampling
        vTaskDelay(pdMS_TO_TICKS(PRESENCE_SAMPLE_INTERVAL_MS));
    }
}

static void esp_zb_task(void *pvParameters)
{
    /* Initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    /* Create customized thermostat endpoint */
    esp_zb_thermostat_cfg_t thermostat_cfg = ESP_ZB_DEFAULT_THERMOSTAT_CONFIG();
    esp_zb_ep_list_t *esp_zb_thermostat_ep = custom_thermostat_ep_create(HA_THERMOSTAT_ENDPOINT, &thermostat_cfg);

    /* Register the device */
    esp_zb_device_register(esp_zb_thermostat_ep);

    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}




void lvgl_task(void *pvParameters)
{
    char buf[64]; // Buffer for displaying temperature and presence status

    while (1)
    {
        lv_task_handler(); 
        // Update temperature label
        snprintf(buf, sizeof(buf), "%.1f°C", g_temperature);
        lv_label_set_text(label_temp, buf);

        // // Update presence label
        // snprintf(buf, sizeof(buf), "%s", g_presence_detected ? "HOME" : "AWAY");
        // lv_label_set_text(label_presence, buf);

        // // Update TRV status label
        // snprintf(buf, sizeof(buf), "HEATING %s", g_trv_is_on ? "ON" : "OFF");
        // lv_label_set_text(label_trv, buf);

        vTaskDelay(pdMS_TO_TICKS(1000)); // Update every second (adjust as needed)
    }
}

void hmmd_read_task(void *arg)
{
    uint8_t data[128];
    while (1)
    {
        ESP_LOGI(TAG, "HMMD: Reading data from HMMD");
        int len = uart_read_bytes(HMMD_UART_NUM, data, sizeof(data) - 1, pdMS_TO_TICKS(100));
        if (len > 0)
        {
            data[len] = '\0'; // null-terminate
            ESP_LOGI(TAG, "HMMD: %s", (char *)data);
            // You can now parse data, e.g., look for keywords like "presence", "distance", etc.
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    LCD_Init();   // Initialize LCD 
    LVGL_Init(); // Initialize LVGL library
    hmmd_uart_init(); // Initialize HMMD UART
 

    Lvgl_smart_stat_ui_close();  // Clean up any existing UI

    Lvgl_smart_stat_ui();// Create LVGL UI elements

    // Create Zigbee main task
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);

    // Create DHT sensor monitoring task
    xTaskCreate(dht_sensor_task, "DHT_sensor", 2048, NULL, 4, NULL);

    //xTaskCreate(TaskFunction_t functionname, const char * const pcName, uint32_t usStackDepth, void *pvParameters, UBaseType_t uxPriority, TaskHandle_t *pvCreatedTask); // Create a task
    xTaskCreate(lvgl_task, "lvgl_task", 8192, NULL, 6, NULL); // Create LVGL task
    ESP_LOGI(TAG, "Creating HMMD read task...");
    esp_err_t ret = xTaskCreate(hmmd_read_task, "hmmd_read_task", 4096, NULL, 7, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create HMMD read task");
    } else {
        ESP_LOGI(TAG, "HMMD read task created successfully");
    }
}
