#include "esp_log.h"
#include "dht.h"
#include "driver/uart.h"
#include "sensors.h"



#define TAG "SENSORS"


// Initialise DHT sensor
 esp_err_t dht_sensor_init(void)
{
    if (g_dht_initialised)
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

    g_dht_initialised = true;
    ESP_LOGI(TAG, "DHT sensor GPIO configured successfully");
    return ESP_OK;
}


 esp_err_t hmmd_uart_init(void)
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
    g_hmmd_initialised = true;
    return ESP_OK;
}

 esp_err_t read_dht_sensor(void)
{
    if (!g_dht_initialised)
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

    // ESP_LOGI(TAG, "DHT22 Readings - Temperature: %.1f°C, Humidity: %.1f%%", temperature, humidity);
    return ESP_OK;
}