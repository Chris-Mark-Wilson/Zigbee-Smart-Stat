#ifndef SENSORS_H
#define SENSORS_H 


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
static bool g_dht_initialised = false; // Flag to indicate if dht temp sensor is initialized
static bool g_hmmd_initialised = false; // Flag to indicate if HMMD sensor is initialised
static float g_temperature = 0;
static float g_humidity = 0;


 esp_err_t dht_sensor_init(void);

 esp_err_t read_dht_sensor(void);

 esp_err_t hmmd_uart_init(void);


#endif // SENSORS_H