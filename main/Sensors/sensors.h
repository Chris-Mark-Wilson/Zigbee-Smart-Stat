#ifndef SENSORS_H
#define SENSORS_H 

#include "esp_err.h"
#include "driver/uart.h"
#include "driver/gpio.h"

// DHT22 sensor settings
#define DHT_GPIO_PIN 4          
#define DHT_TYPE DHT_TYPE_AM2301   
#define DHT_READ_INTERVAL_MS 10000 

// RCWL-0516 Presence sensor settings
#define RCWL_GPIO_PIN 5               
#define PRESENCE_DEBOUNCE_COUNT 3      
#define PRESENCE_SAMPLE_INTERVAL_MS 50 

#define HMMD_UART_NUM      UART_NUM_1
#define HMMD_UART_RX_PIN   GPIO_NUM_17  
#define HMMD_UART_TX_PIN   UART_PIN_NO_CHANGE 

// Global variables declarations (removed initializations)
extern float g_temperature;
extern float g_humidity;
extern bool g_dht_initialised;
extern bool g_hmmd_initialised;

// Function declarations
esp_err_t dht_sensor_init(void);
esp_err_t read_dht_sensor(void);
esp_err_t hmmd_uart_init(void);

#endif // SENSORS_H