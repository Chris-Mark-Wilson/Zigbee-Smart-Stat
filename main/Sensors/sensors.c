#include "esp_log.h"
#include "dht.h"
#include "driver/uart.h"





// DHT22 sensor settings
#define DHT_GPIO_PIN 4          // GPIO pin connected to DHT22 sensor
#define DHT_TYPE DHT_TYPE_AM2301   // DHT22 and AM2301 are the same
#define DHT_READ_INTERVAL_MS 10000 // Read every 10 seconds
// RCWL-0516 Presence sensor settings
#define RCWL_GPIO_PIN 5               // GPIO pin connected to RCWL presence sensor
#define PRESENCE_DEBOUNCE_COUNT 3      // Number of consecutive readings needed to change state
#define PRESENCE_SAMPLE_INTERVAL_MS 50 // Time between samples for debounce

#define HMMD_UART_NUM      UART_NUM_1
#define HMMD_UART_RX_PIN   GPIO_NUM_17  // Connect HMMD TX here
#define HMMD_UART_TX_PIN   UART_PIN_NO_CHANGE // Only if you need to send config


//global variables
static bool g_dht_initialized = false; // Flag to indicate if dht temp sensor is initialized
static bool g_rcwl_initialized = false;  // Flag to indicate if rcwl presence sensor is initialized



#define TAG "SENSORS"


// Initialise DHT sensor
static esp_err_t dht_sensor_init(void)
{
    if (g_dht_initialized)
    {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Configuring DHT sensor on GPIO %d", DHT_GPIO_PIN);

    // Configure GPIO (the DHT library will handle the mode changes during readings)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DHT_GPIO_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD, // Open drain for DHT22
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO configuration failed: %s", esp_err_to_name(ret));
        return ret;
    }

    g_dht_initialized = true;
    ESP_LOGI(TAG, "DHT sensor GPIO configured successfully");
    return ESP_OK;
}

// rcwl 0516 presence sensor initialization
static esp_err_t rcwl_sensor_init(void)
{
    if (g_rcwl_initialized)
    {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Configuring RCWL presence sensor on GPIO %d", RCWL_GPIO_PIN);

    // Configure GPIO as input (sensor outputs HIGH when presence detected)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RCWL_GPIO_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // Pull-down when no signal
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO configuration for RCWL failed: %s", esp_err_to_name(ret));
        return ret;
    }

    g_rcwl_initialized = true;
    ESP_LOGI(TAG, "RCWL presence sensor configured successfully");
    return ESP_OK;


}

void hmmd_uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,  // Check your sensor's default rate
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_driver_install(HMMD_UART_NUM, 1024, 0, 0, NULL, 0);
    uart_param_config(HMMD_UART_NUM, &uart_config);
    uart_set_pin(HMMD_UART_NUM, HMMD_UART_TX_PIN, HMMD_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "HMMD UART sensor initialized on RX GPIO %d", HMMD_UART_RX_PIN);
}