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
#include "zcl/esp_zigbee_zcl_thermostat.h"
#include "zcl/esp_zigbee_zcl_command.h"

// #include "ha/esp_zigbee_ha_standard.h"

// lcd headers
#include "LVGL_Driver/LVGL_Driver.h"

// zigbee headers

#include "Zigbee/zigbee.h"
#include "Buttons/button.h"
#include "LVGL_UI/ui_screens.h"
#include "LVGL_UI/ui_events.h"
// sensors headers
#include "Sensors/sensors.h"
// settings headers
#include "Settings/settings.h"
#include "Helpers/helpers.h"

// Global state tracking variables
int64_t g_last_presence_time = 0; // Last time presence was detected
bool g_is_window_open = false;           // Global window state
bool g_presence_detected = false; // Global presence state
bool g_trv_state = false;         // Track TRV state

// Temperature control variables (in hundredths of degrees)


#if defined ZB_ED_ROLE
#error Define ZB_COORDINATOR_ROLE in idf.py menuconfig to compile source code.
#endif

// Create queue handle for UI events
QueueHandle_t ui_event_queue;

// this exact function name MUST remain in this file
//  it is used by the zigbee library to call the signal handler
// and it expects to find it in here or it just wont fucking work
// its so i can put all my zigbee code in one file and not have to worry about it
//  being in the right place
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    zigbee_signal_handler(signal_struct);
}


// handle window sensor state change
// static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
// {
//     static const char *TAG = "ZB_ATTR_HANDLER";
//     ESP_LOGI(TAG, "Attribute handler called with message: %p", message);
//     esp_err_t ret = ESP_OK;

//     if (!message)
//     {
//         ESP_LOGE(TAG, "Empty message received");
//         return ESP_FAIL;
//     }

//     ESP_LOGI(TAG, "Attribute details:");
//     ESP_LOGI(TAG, "  Endpoint: %d", message->info.dst_endpoint);
//     ESP_LOGI(TAG, "  Cluster: 0x%04x", message->info.cluster);
//     ESP_LOGI(TAG, "  Attribute ID: 0x%04x", message->attribute.id);
//     ESP_LOGI(TAG, "  Status: 0x%02x", message->info.status);

//     // Handle any endpoint (not just endpoint 1)
//     if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE)
//     {
//         if (message->attribute.id == ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID)
//         {
//             uint16_t zone_status = *(uint16_t *)message->attribute.data.value;
//             ESP_LOGI(TAG, "  Zone status: 0x%04x", zone_status);
//             g_is_window_open = (zone_status & 0x0001);

//             // Trigger UI update
//             ui_event_t event = {
//                 .target_screen = SCREEN_MAIN,
//                 .message = ""};
//             xQueueSend(ui_event_queue, &event, 0);
//         }
//     }

//     return ret;
// }
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    static const char *TAG = "ZB_ACTION_HANDLER";
    esp_err_t ret = ESP_OK;

    // Enhanced callback logging
    // ESP_LOGI(TAG, "Action handler called with callback ID: 0x%04x, message: %p", callback_id, message);

    switch (callback_id)
    {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ESP_LOGI(TAG, "SET_ATTR_VALUE callback received");
        // ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;

    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID: // 0x1000
        ESP_LOGI(TAG, "Read attribute response received");
        if (!message)
        {
            ESP_LOGW(TAG, "Null message received");
            break;
        }

        const esp_zb_zcl_cmd_read_attr_resp_message_t *resp =
            (esp_zb_zcl_cmd_read_attr_resp_message_t *)message;

        ESP_LOGI(TAG, "Response cluster: 0x%04x", resp->info.cluster);

        // Validate response structure
        if (!resp->variables)
        {
            ESP_LOGW(TAG, "Null variables in response");
            break;
        }

        if (resp->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE)
        {
            if (resp->variables->attribute.id == ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID)
            {
                // Validate attribute data
                if (!resp->variables->attribute.data.value)
                {
                    ESP_LOGW(TAG, "Null attribute value");
                    break;
                }

                // Now safe to access the value
                uint16_t zone_status = *(uint16_t *)resp->variables->attribute.data.value;
                ESP_LOGI(TAG, "IAS Zone Status: 0x%04x", zone_status);

                // Bit 0 indicates alarm1 (usually the open/closed state)
                g_is_window_open = (zone_status & 0x0001);
                ESP_LOGI(TAG, "Window is %s", g_is_window_open ? "OPEN" : "CLOSED");

                // Decode other status bits if needed
                bool alarm2 = (zone_status & 0x0002) != 0;
                bool tamper = (zone_status & 0x0004) != 0;
                bool battery = (zone_status & 0x0008) != 0;

                ESP_LOGI(TAG, "Additional status - Alarm2: %d, Tamper: %d, Battery Low: %d",
                         alarm2, tamper, battery);

                // Trigger UI update
                ui_event_t event = {
                    .target_screen = SCREEN_MAIN,
                    .message = ""};
                xQueueSend(ui_event_queue, &event, 0);
            }
        }
        break;

    case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
        // ESP_LOGI(TAG, "REPORT_ATTR callback received");
        if (message)
        {
            const esp_zb_zcl_report_attr_message_t *report =
                (esp_zb_zcl_report_attr_message_t *)message;
   
            // First check if device is known and authorized
            zigbee_device_t *device = get_device_by_short_addr(report->src_address.u.short_addr);
            if (!device) {
                ESP_LOGW(TAG, "Received report from unknown device 0x%04x - ignoring",
                         report->src_address.u.short_addr);
                break;
            }

            // Check if device is in our stored devices list
            if (report->cluster == ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE)
            {
                if (report->attribute.id == ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID)
                {
                    // Process status only from known devices
                    uint16_t zone_status = *(uint16_t *)report->attribute.data.value;
                    ESP_LOGI(TAG, "Zone status from authorized device: 0x%04x", zone_status);
                    g_is_window_open = (zone_status & 0x0001);

                    // Trigger UI update
                    ui_event_t event = {
                        .target_screen = SCREEN_MAIN,
                        .message = ""};
                    xQueueSend(ui_event_queue, &event, 0);
                }
            }
        }
        break;

    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
        ESP_LOGI(TAG, "DEFAULT_RESP callback received");
        if (message)
        {
            const esp_zb_zcl_cmd_default_resp_message_t *resp =
                (esp_zb_zcl_cmd_default_resp_message_t *)message;
            ESP_LOGI(TAG, "Default response details:");
            ESP_LOGI(TAG, "  Status: 0x%02x", resp->status_code);
            ESP_LOGI(TAG, "  Cluster: 0x%04x", resp->info.cluster);
            ESP_LOGI(TAG, "  Command ID: 0x%02x", resp->info.command.id); // Add this

            if (resp->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE)
            {
                ESP_LOGI("IAS ZONE: ", "IAS Zone command response:");
                if (resp->status_code == ESP_ZB_ZCL_STATUS_SUCCESS)
                {
                    ESP_LOGI(TAG, "Command accepted by sensor");
                }
                else
                {
                    ESP_LOGW(TAG, "Command rejected with status: 0x%02x", resp->status_code);
                }
            }
        }
        break;

    case ESP_ZB_CORE_CMD_WRITE_ATTR_RESP_CB_ID: // 0x1001
        ESP_LOGI(TAG, "Write attribute response received");
        if (message)
        {
            const esp_zb_zcl_cmd_write_attr_resp_message_t *resp =
                (esp_zb_zcl_cmd_write_attr_resp_message_t *)message;
            ESP_LOGI(TAG, "Write response status: 0x%02x", resp->info.status);
        }
        break;

    case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID:
        ESP_LOGI("IAS CONFIG", "Report config response received");
        if (message)
        {
            const esp_zb_zcl_cmd_write_attr_resp_message_t *resp =
                (esp_zb_zcl_cmd_write_attr_resp_message_t *)message;
            if (resp->info.status == ESP_ZB_ZCL_STATUS_SUCCESS)
            {
                ESP_LOGI(TAG, "Report configuration accepted by sensor");
            }
            else
            {
                ESP_LOGE(TAG, "Report configuration rejected with status: 0x%02x", resp->info.status);
            }
            ESP_LOGI(TAG, "Report config response details:");
            ESP_LOGI(TAG, "  Status: 0x%02x", resp->info.status);
            ESP_LOGI(TAG, "  Cluster: 0x%04x", resp->info.cluster);
        }
        break;

    case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION:
        ESP_LOGI(TAG, "Device leave indication received in action handler");
        break;

case ESP_ZB_CORE_CMD_IAS_ZONE_ZONE_STATUS_CHANGE_NOT_ID:
    ESP_LOGI("IAS Zone Action Handler", "IAS Zone status change notification received");
    if (message)
    {
        const esp_zb_zcl_ias_zone_status_change_notification_message_t *status =
            (esp_zb_zcl_ias_zone_status_change_notification_message_t *)message;
            
        // First verify device is authorized
        zigbee_device_t *device = get_device_by_short_addr(status->info.src_address.u.short_addr);
        if (!device) {
            ESP_LOGW(TAG, "Status change from unknown device 0x%04x - ignoring", 
                     status->info.src_address.u.short_addr);
            break;
        }

        ESP_LOGI("IAS Zone Action Handler", "Zone status from device 0x%04x: 0x%04x", 
                 status->info.src_address.u.short_addr, status->zone_status);
         
        g_is_window_open = (status->zone_status & 0x0001);

        // Trigger UI update
        ui_event_t event = {
            .target_screen = SCREEN_MAIN,
            .message = ""
        };
        xQueueSend(ui_event_queue, &event, 0);
    }
    break;
       

    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        ESP_LOGI(TAG, "Device announce signal received in action handler");
        break;

    default:
        ESP_LOGW(TAG, "Unhandled Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

// TODO implement when everything is bound and getting data
static void evaluate_control_logic(void) {
//     if(!g_presence_detected){
//         // No presence detected - turn off TRV
//         if(g_trv_state) {
//             turn_trvs_off();
//         }
//         return;
//     }
//     if(g_is_window_open) {
//         // If window is open, turn off TRV
//         if(g_trv_state) {
//             turn_trvs_off();
//         }
//         return;
//     }
//     //if temp > temp max turn off trv
//     if(g_temperature>=g_target_high_temp && g_trv_state) {
//         turn_trvs_off();
//         return;
//     }
//     //if presence detected && windows are closed && temp < temp max turn on trv
//     if(g_presence_detected && !g_is_window_open && g_temperature < g_target_high_temp) {
//         turn_trvs_on();
//         return;
//     }
//     //if temp < temp min && windows are closed turn on trv 
//     if(g_temperature < g_target_low_temp && !g_is_window_open) {
//         turn_trvs_on();
//         return;
//     }
// return;
}

static void lvgl_task(void *pvParameters)
{
    while (1)
    {
        lv_task_handler();             // Keep this as it's needed for LVGL core functionality
        vTaskDelay(pdMS_TO_TICKS(10)); // Reduced delay for smoother updates
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
            // ESP_LOGI(TAG, "DHT Readings - Temperature: %.1f°C, Humidity: %.1f%%", g_temperature, g_humidity);
            ui_event_t event = {
                .target_screen = SCREEN_MAIN,
                .message = ""};
            xQueueSend(ui_event_queue, &event, 0);
        }

        // Shorter delay for presence sampling
        vTaskDelay(pdMS_TO_TICKS(PRESENCE_SAMPLE_INTERVAL_MS));
    }
}

// do not delete, needed to bind trv for two way communication
// Adds IAS Zone cluster to the endpoint
// and adds the temperature measurement cluster for attribute reporting
static esp_zb_cluster_list_t *custom_thermostat_clusters_create(esp_zb_thermostat_cfg_t *thermostat)
{
    // esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    // esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&(thermostat->basic_cfg));
    // ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME));
    // ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER));
    // ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    // ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(&(thermostat->identify_cfg)), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    // ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));
    // ESP_ERROR_CHECK(esp_zb_cluster_list_add_thermostat_cluster(cluster_list, esp_zb_thermostat_cluster_create(&(thermostat->thermostat_cfg)), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    // /* Add temperature measurement cluster for attribute reporting */
    // ESP_ERROR_CHECK(esp_zb_cluster_list_add_temperature_meas_cluster(cluster_list, esp_zb_temperature_meas_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));

    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    
    // Basic cluster is required for proper device identification
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&(thermostat->basic_cfg));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    
    // Thermostat cluster for mode control (ON/OFF) needs to be a client role to control trvs apparently
    // ESP_ERROR_CHECK(esp_zb_cluster_list_add_thermostat_cluster(cluster_list, 
    //     esp_zb_thermostat_cluster_create(&(thermostat->thermostat_cfg)), 
    //     ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
     // Thermostat CLIENT cluster — REQUIRED to control TRVs, this will now be done using occupied heating setpoint in turn_trvs_on() in helpers 
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_thermostat_cluster(
        cluster_list,
        esp_zb_thermostat_cluster_create(&(thermostat->thermostat_cfg)),
        ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));

    return cluster_list;
}
// do not delete, needed to bind trv for two way communication and IAS zone cluster, (intruder alarm system)
// creates the endpoint and adds the clusters to it

// Separate cluster creation for IAS Zone endpoint
static esp_zb_cluster_list_t *create_ias_zone_clusters(void)
{
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    // Create empty attribute list for IAS Zone client
    esp_zb_attribute_list_t *ias_zone_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE);

    ESP_ERROR_CHECK(esp_zb_cluster_list_add_ias_zone_cluster(cluster_list,
                                                             ias_zone_cluster, // Use empty list instead of NULL
                                                             ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));

    return cluster_list;
}
static esp_zb_ep_list_t *create_endpoints(esp_zb_thermostat_cfg_t *thermostat)
{
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();

    // First endpoint (10) - Thermostat
    esp_zb_endpoint_config_t thermostat_ep_config = {
        .endpoint = HA_THERMOSTAT_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID,
        .app_device_version = 0};
    esp_zb_ep_list_add_ep(ep_list, custom_thermostat_clusters_create(thermostat), thermostat_ep_config);

    // Second endpoint (1) - IAS Zone Client
    esp_zb_endpoint_config_t ias_ep_config = {
        .endpoint = IAS_ZONE_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_IAS_ZONE_ID,
        .app_device_version = 0};
    esp_zb_ep_list_add_ep(ep_list, create_ias_zone_clusters(), ias_ep_config);

    return ep_list;
}

static void zigbee_task(void *pvParameters)
{
    


   

    ESP_LOGW("ZIGBEE TASK semaphore", "passed semaphore, continuing initialization"); 

    ui_switch_screen(SCREEN_BOOT);
    char settings_message[100];
  snprintf(settings_message, sizeof(settings_message), 
             "Settings loaded: \nHigh Temp: %d°C\n Low Temp: %d°C\n Range Limit: %d m",
             g_target_high_temp, g_target_low_temp, g_range_limit);
    
    ui_event_t event = {
        .target_screen = SCREEN_BOOT
    };
    // Copy the string into event.message
    strncpy(event.message, settings_message, sizeof(event.message) - 1);
    event.message[sizeof(event.message) - 1] = '\0';  // Ensure null termination
    
    xQueueSend(ui_event_queue, &event, portMAX_DELAY);
         vTaskDelay(LONG_DELAY);

ESP_LOGW("ZIGBEE TASK SETTINGS DEBUG", "Settings loaded: Target High Temp: %d, Target Low Temp: %d, Range Limit: %d",
             g_target_high_temp, g_target_low_temp, g_range_limit);


    // Before initialization
    // Set default values
    ESP_ERROR_CHECK(esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK));


    // Initialize Zigbee stack
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
    esp_zb_init(&zb_nwk_cfg);



    /* Create customized thermostat endpoint */
    esp_zb_thermostat_cfg_t thermostat_cfg = ESP_ZB_DEFAULT_THERMOSTAT_CONFIG();
    esp_zb_ep_list_t *esp_zb_endpoints = create_endpoints(&thermostat_cfg);

    /* Register the device */
    esp_zb_device_register(esp_zb_endpoints);


    esp_zb_core_action_handler_register(zb_action_handler);

    // Start Zigbee stack with autostart
    ESP_ERROR_CHECK(esp_zb_start(false));

    // Enter Zigbee main loop
    esp_zb_stack_main_loop();
}
// ...existing code...
// ...existing code...
// ...existing code...
// ...existing code...
static void hmmd_read_task(void *arg)
{
    if (!g_hmmd_initialised)
    {
        ESP_LOGE(TAG, "HMMD sensor not initialized");
        return;
    }
    uint8_t data[128];
    char *range_str;

    ESP_LOGI("HMMD READ TASK", "HMMD task started with initial range limit: %d m", g_range_limit);

    while (1)
    {
        int len = uart_read_bytes(HMMD_UART_NUM, data, sizeof(data) - 1, pdMS_TO_TICKS(100));

        if (len > 0)
        {
            data[len] = '\0';

            // Look for "Range " instead of "range:"
            range_str = strstr((char *)data, "Range ");
            if (range_str)
            {
                float range_cm;
                if (sscanf(range_str, "Range %f", &range_cm) == 1)
                {
                    g_current_range = range_cm; // Update the global range variable defined in ui_screens.h
                    // Update presence state

                    g_presence_detected = (range_cm < g_range_limit* 100.0f); // Convert limit to cm for comparison

                    // Always trigger UI update when we get valid range data
                    ui_event_t event = {
                        .target_screen = SCREEN_MAIN,
                        .message = ""};
                    xQueueSend(ui_event_queue, &event, 0);
                }
                else
                {
                    ESP_LOGW(TAG, "Failed to parse range value from: %s", range_str);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}



static void ui_update_task(void *pvParameters)
{
    ui_event_t event;

    while (1)
    {
        if (xQueueReceive(ui_event_queue, &event, portMAX_DELAY))
        {
            // Handle UI update in LVGL context
            switch (event.target_screen)
            {
            case SCREEN_BOOT:
            char header_label[40];
                snprintf(header_label, sizeof(header_label), "TRV's: %d\nWindows: %d", trv_count,window_sensor_count);

                ui_update_boot_status(event.message,header_label);
                                      
                break;
            case SCREEN_MAIN:
                ui_update_main_screen(
                    g_temperature > 0 ? g_temperature : 0.0f,
                    g_humidity > 0 ? g_humidity : 0.0f,
                    g_presence_detected,
                    g_is_window_open,
                g_trv_state);
                break;

            case SCREEN_SETTINGS:
                ui_update_settings(
                    g_target_high_temp,
                    g_target_low_temp,
                    g_range_limit,
                    g_room);
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

    // LCD_Init();
    LVGL_Init();


    // Create UI event queue first
    ui_event_queue = xQueueCreate(10, sizeof(ui_event_t));
    if (ui_event_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create UI event queue");
        return;
    }

    // Initialize UI screens
    ui_init_screens(); //sets up the various screens and their elements
    settings_init_callbacks(); // Initialize settings callbacks called when sliders are moved etc 
    // Create tasks after queue is initialized
    xTaskCreate(lvgl_task, "lvgl_handler", 4096, NULL, 6, NULL);
    xTaskCreate(ui_update_task, "ui_update", 4096, NULL, 5, NULL);

    static SemaphoreHandle_t settings_complete = NULL;
    settings_complete = xSemaphoreCreateBinary();//create traffic light red
    settings_register_callback(settings_complete_cb, settings_complete);

    //attempt to load settings from nvs
    if(load_settings_from_nvs() != ESP_OK) {
            ESP_LOGE("MAIN semaphore", "Failed to load settings from NVS");
        ui_switch_screen(SCREEN_SETTINGS);//save button will release the semaphore with new settings, cancel will release with default settings
        // Wait for settings to be saved or cancelled
        ESP_LOGI("MAIN TASK semaphore", "Waiting for settings to be saved or cancelled");
        xSemaphoreTake(settings_complete, portMAX_DELAY); // Wait here for settings
        ESP_LOGI("MAIN semaphore", "Settings saved or cancelled, continuing initialization");
    } else {
    ESP_LOGI(TAG, "Settings loaded from NVS");
    
       
}

    esp_err_t ret = hmmd_uart_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE("UART init", "Failed to initialize HMMD UART");
        return;
    }
    else
    {
        ESP_LOGI("UART init", "HMMD UART initialized successfully");
        g_hmmd_initialised = true;
    }

    xTaskCreate(zigbee_task, "Zigbee_main", 4096, NULL, 5, NULL);
    xTaskCreate(hmmd_read_task, "HMMD_read", 2048, NULL, 4, NULL);
    xTaskCreate(dht_sensor_task, "DHT_sensor", 2048, NULL, 4, NULL);
}
