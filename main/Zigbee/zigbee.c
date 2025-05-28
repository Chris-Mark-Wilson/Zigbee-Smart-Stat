#include "nvs.h"
#include "nvs_flash.h"
#include "Zigbee/zigbee.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "zcl/esp_zigbee_zcl_ias_zone.h" // Added for IAS Zone specific definitions
#include "esp_timer.h"
#include "../LVGL_UI/ui_screens.h"
#include "../LVGL_UI/ui_events.h"
#include "../Helpers/helpers.h"
#include "../NVS/nvs_functions.h"



#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof(arr[0]))



static const char *TAG = "ZIGBEE";

// Define the global array
zigbee_device_t stored_devices[MAX_TRV_DEVICES + MAX_WINDOW_SENSORS];

uint8_t stored_device_count = 0;
uint8_t trv_count = 0;
uint8_t window_sensor_count = 0;
uint8_t devices_left=0; //counter for devices unbound when restarting
bool network_open = false;
static uint8_t last_config_tsn = 0; // tracking the last transaction sequence number for config report command
static bool found_window_sensor = false;
static bool found_trv = false;
extern QueueHandle_t ui_event_queue;











esp_err_t open_network(uint16_t duration)
{
    if (esp_zb_bdb_open_network(duration) != ESP_OK)
    {
        return ESP_FAIL;
    }
    network_open = true;
    ESP_LOGI(TAG, "Network opened for %d seconds", duration);
    return ESP_OK;
}

esp_err_t close_network(void)
{
    if (esp_zb_bdb_close_network() != ESP_OK)
    {
        return ESP_FAIL;
    }
    network_open = false;
    ESP_LOGI(TAG, "Network closed");
    return ESP_OK;
}

bool is_network_open(void)
{
    return network_open;
}

void esp_zb_zcl_config_report_cb(esp_zb_zcl_command_send_status_message_t message)
{
    if (message.tsn == last_config_tsn)
    {
        if (message.status == ESP_OK)
        {
            ESP_LOGI(TAG, "Config report command accepted by sensor");
        }
        else
        {
            ESP_LOGE(TAG, "Config report command failed: %s", esp_err_to_name(message.status));
        }
    }
}

static void bind_trv_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx)
{
    esp_zb_zdo_bind_req_param_t *bind_req = (esp_zb_zdo_bind_req_param_t *)user_ctx;

    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS)
    {
        /* Local binding succeeds */
        // if (bind_req->req_dst_addr == esp_zb_get_short_address())
        // {
            ESP_LOGI(TAG, "Trv bound to controller from address(0x%x) on trv endpoint(%d)",
                     bind_req->req_dst_addr, bind_req->dst_endp);

        //     // /* Read peer Manufacture Name & Model Identifier */
        //     // esp_zb_zcl_read_attr_cmd_t read_req = {0};
        //     // read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
        //     // read_req.zcl_basic_cmd.src_endpoint = HA_THERMOSTAT_ENDPOINT;
        //     // read_req.zcl_basic_cmd.dst_endpoint = trv.endpoint;
        //     // read_req.zcl_basic_cmd.dst_addr_u.addr_short = trv.short_addr;
        //     // read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_BASIC;

        //     uint16_t attributes[] = {
        //         ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
        //         ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
        //     };
        //     read_req.attr_number = ARRAY_LENGTH(attributes);
        //     read_req.attr_field = attributes;

        //     esp_zb_zcl_read_attr_cmd_req(&read_req);
        // }
        // if (bind_req->req_dst_addr == trv.short_addr)
        // {
        //     ESP_LOGI(TAG, "Controller bound to trv from sensor address(0x%x) on sensor endpoint(%d)",
        //              trv.short_addr, trv.endpoint);
        // }
        free(bind_req);
    }
    else
    {
        /* Bind failed, maybe retry the binding ? */

        // esp_zb_zdo_device_bind_req(bind_req, bind_cb, bind_req);
    }
}
static void find_trv_cb(esp_zb_zdp_status_t zdo_status, uint16_t peer_addr, uint8_t peer_endpoint, void *user_ctx)
{
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS)
    {
        if (peer_addr == 0xffff || peer_addr == 0x0000)
        {
            ESP_LOGE("FIND TRV CALLBACK", "Invalid peer address");
            return;
        }
        if (peer_endpoint == 0)
        {
            ESP_LOGE("FIND TRV CALLBACK", "Invalid peer endpoint");
            return;
        }

        ESP_LOGI("FIND TRV CALLBACK", "Device identified as TRV");
        ESP_LOGI("FIND TRV CALLBACK", "Found TRV device at address 0x%04x, endpoint %d", peer_addr, peer_endpoint);

        // // Store device info with correct address
        // zigbee_device_t *trv = (trv_device_params_t *)user_ctx;
        // if (!sensor)
        // {
        //     ESP_LOGE(TAG, "Invalid user context");
        //     return;
        // }
        // sensor->endpoint = peer_endpoint;
        // sensor->short_addr = peer_addr;
        // // esp_zb_ieee_address_by_short(sensor->short_addr, sensor->ieee_addr);

        // /* 1. Send binding request to the trv */
        // esp_zb_zdo_bind_req_param_t *bind_req = (esp_zb_zdo_bind_req_param_t *)calloc(1, sizeof(esp_zb_zdo_bind_req_param_t));

        // bind_req->req_dst_addr = peer_addr;



        /* 2. Send binding request to self */
       esp_zb_zdo_bind_req_param_t *bind_req = (esp_zb_zdo_bind_req_param_t *)calloc(1, sizeof(esp_zb_zdo_bind_req_param_t));

        bind_req->req_dst_addr = esp_zb_get_short_address();

        /* populate the src information of the binding */
        esp_zb_get_long_address(bind_req->src_address);
        bind_req->src_endp = HA_THERMOSTAT_ENDPOINT;
        bind_req->cluster_id = ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT;

        /* populate the dst information of the binding */
        bind_req->dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
        memcpy(bind_req->dst_address_u.addr_long, user_ctx, sizeof(esp_zb_ieee_addr_t));
        bind_req->dst_endp = peer_endpoint;

        ESP_LOGI(TAG, "Binding coordinator to trv (send commands)");
        esp_zb_zdo_device_bind_req(bind_req, bind_trv_cb, bind_req);

        /* 3. save device to nvs */

        device_type_t dev_type = DEVICE_TYPE_TRV;

        // Prepare the device structure
        zigbee_device_t new_device = {
            .type = DEVICE_TYPE_TRV,
            .short_addr = peer_addr,
            .endpoint = peer_endpoint,
        };
        memcpy(new_device.ieee_addr, user_ctx, sizeof(esp_zb_ieee_addr_t));

        snprintf(new_device.name, MAX_DEVICE_NAME_LENGTH, "%s_%d",
                 dev_type == DEVICE_TYPE_TRV ? "TRV" : "WINDOW",
                 dev_type == DEVICE_TYPE_TRV ? ++trv_count : ++window_sensor_count);

        // Save the device to
        if (save_device_to_nvs(&new_device) == ESP_OK)
        {
            ui_event_t event = {
                .target_screen = SCREEN_BOOT,
                .message = "Device paired successfully"};
            xQueueSend(ui_event_queue, &event, 0);
            ESP_LOGI(TAG, "Device saved as %s", new_device.name);
        }
    }
}

static void find_trv(esp_zb_zdo_match_desc_req_param_t *param, esp_zb_zdo_match_desc_callback_t callback, void *user_ctx)
{
    uint16_t cluster_list[] = {ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT};
    param->profile_id = ESP_ZB_AF_HA_PROFILE_ID;
    param->num_in_clusters = 1;
    param->num_out_clusters = 0;
    param->cluster_list = cluster_list;
    esp_zb_zdo_match_cluster(param, callback, user_ctx);
}

void read_window_sensor_status(uint16_t addr, uint8_t endpoint)
{
    // Define the attribute ID we want to read
    uint16_t attr_id = ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID;

    esp_zb_zcl_read_attr_cmd_t read_req = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = addr,
            .dst_endpoint = endpoint,
            .src_endpoint = HA_THERMOSTAT_ENDPOINT,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE,
        .attr_number = 1,
        .attr_field = &attr_id,
    };

    ESP_LOGI(TAG, "Reading window sensor status");

    int tsn = esp_zb_zcl_read_attr_cmd_req(&read_req);

    if (tsn == 0)
    {
        ESP_LOGW(TAG, "Read attribute command returned 0 TSN - this may be normal");
    }
    else
    {
        ESP_LOGI(TAG, "Read attribute command returned TSN: %d", tsn);
    }
}
static void bind_window_sensor_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx)
{
    esp_zb_zdo_bind_req_param_t *bind_req = (esp_zb_zdo_bind_req_param_t *)user_ctx;

    ESP_LOGI(TAG, "Window sensor binding callback triggered with status: 0x%x", zdo_status);
    ESP_LOGI(TAG, "Bind request details - dst: 0x%04x, src_ep: %d, dst_ep: %d",
             bind_req->req_dst_addr, bind_req->src_endp, bind_req->dst_endp);

    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS)
    {
        ESP_LOGI(TAG, "Window sensor bound successfully");

       

        read_window_sensor_status(stored_devices[window_sensor_count - 1].short_addr,
                                  stored_devices[window_sensor_count - 1].endpoint);
    }
    else
    {
        ESP_LOGE(TAG, "Window sensor binding failed with status: 0x%x", zdo_status);
    }
    free(bind_req);
}
static void find_window_sensor_cb(esp_zb_zdp_status_t zdo_status, uint16_t peer_addr, uint8_t peer_endpoint, void *user_ctx)
{



    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS)
    {

        found_window_sensor = true;
        if (peer_addr == 0xFFFF || peer_endpoint == 0xFF)
        {
            ESP_LOGW("FIND_WINDOW_SENSOR_CALLBACK", "Invalid address/endpoint received despite success status");
            found_window_sensor = false;
            return;
        }
        ESP_LOGI("FIND_WINDOW_SENSOR_CALLBACK", "Device identified as Window Sensor");
        ESP_LOGI("FIND_WINDOW_SENSOR_CALLBACK", "Found Window Sensor at address 0x%04x, endpoint %d", peer_addr, peer_endpoint);

        // Send binding request
        esp_zb_zdo_bind_req_param_t *bind_req = (esp_zb_zdo_bind_req_param_t *)calloc(1, sizeof(esp_zb_zdo_bind_req_param_t));
        if (!bind_req)
        {
            ESP_LOGE("FIND_WINDOW_SENSOR_CALLBACK", "Failed to allocate bind request");
            return;
        }

        // Only proceed if device isn't already bound
        if (!device_exists(peer_addr))
        {
            bind_req->req_dst_addr = peer_addr;
            esp_zb_ieee_address_by_short(peer_addr, bind_req->src_address);
            bind_req->src_endp = peer_endpoint;
            bind_req->cluster_id = ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE;
            bind_req->dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
            esp_zb_get_long_address(bind_req->dst_address_u.addr_long);
            bind_req->dst_endp = IAS_ZONE_ENDPOINT;

            ESP_LOGI("FIND_WINDOW_SENSOR_CALLBACK", "Binding Window Sensor to coordinator");
            ESP_LOGI("FIND_WINDOW_SENSOR_CALLBACK", "Sending bind request (IAS Zone) - addr: 0x%04x, ep: %d, cluster: 0x%04x",
                     peer_addr, peer_endpoint, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE);
            esp_zb_zdo_device_bind_req(bind_req, bind_window_sensor_cb, bind_req);

            // Save device
            zigbee_device_t new_device = {
               
                .type = DEVICE_TYPE_WINDOW_SENSOR,
                .short_addr = peer_addr,
                .endpoint = peer_endpoint,
                .last_seen = esp_timer_get_time() / 1000};
     
        memcpy(new_device.ieee_addr, user_ctx, sizeof(esp_zb_ieee_addr_t));

            snprintf(new_device.name, MAX_DEVICE_NAME_LENGTH, "WINDOW_%d", ++window_sensor_count);

            if (save_device_to_nvs(&new_device) == ESP_OK)
            {
                ui_event_t event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "Window sensor paired successfully"};
                xQueueSend(ui_event_queue, &event, 0);
                ESP_LOGI("FIND_WINDOW_SENSOR_CALLBACK", "Window sensor saved as %s", new_device.name);
            }
        }
    }
    else
    {
        ESP_LOGW("FIND_WINDOW_SENSOR_CALLBACK", "Window sensor match failed with status: 0x%x", zdo_status);
    }
}

static void find_window_sensor(esp_zb_zdo_match_desc_req_param_t *param, esp_zb_zdo_match_desc_callback_t callback, void *user_ctx)
{
    // uint16_t cluster_list[] = {ESP_ZB_ZCL_CLUSTER_ID_ON_OFF}; // Window sensors typically use OnOff cluster

    uint16_t cluster_list[] = {
        ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, // 0x0500
        // ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT,    // 0x000F
        // ESP_ZB_ZCL_CLUSTER_ID_DOOR_LOCK        // 0x0101
    };

    param->profile_id = ESP_ZB_AF_HA_PROFILE_ID;
    param->num_in_clusters = 1;
    param->num_out_clusters = 0;
    param->cluster_list = cluster_list;

    ESP_LOGI(TAG, "Trying to identify window sensor at addr 0x%04x", param->dst_nwk_addr);
    ESP_LOGI(TAG, "Using profile 0x%04x, searching for window sensor clusters", param->profile_id);

    esp_err_t err = esp_zb_zdo_match_cluster(param, callback, user_ctx);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send window sensor match descriptor request: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Window sensor match descriptor request sent successfully");
    }
}

void zigbee_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;

    switch (sig_type)
    {

    case ESP_ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
    { // Signal 23 (0x17)
        ui_event_t event = {
            .target_screen = SCREEN_BOOT,
            .message = "Starting Zigbee stack..."};
        xQueueSend(ui_event_queue, &event, portMAX_DELAY);
        vTaskDelay(SHORT_DELAY);
        ESP_LOGI(TAG, "Production config ready, starting initialization");
        break;
    }

    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
    { // Signal 6 (0x06)
        ESP_LOGI(TAG, "Skip startup signal received with status: 0x%x", signal_struct->esp_err_status);
        ui_event_t event = {
            .target_screen = SCREEN_BOOT,
            .message = "Starting network formation..."};
        xQueueSend(ui_event_queue, &event, portMAX_DELAY);
         vTaskDelay(SHORT_DELAY);
        ESP_LOGI(TAG, "Zigbee stack initialized, starting network formation");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
        break;
    }

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    {
        ui_event_t event = {
            .target_screen = SCREEN_BOOT,
            .message = "Forming network..."};
        xQueueSend(ui_event_queue, &event, 0);
        ESP_LOGI(TAG, "First start, forming network");
         vTaskDelay(SHORT_DELAY);
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
        break;
    }

    case ESP_ZB_BDB_SIGNAL_FORMATION:
    {
        if (signal_struct->esp_err_status == ESP_OK)
        {
            ui_event_t event = {
                .target_screen = SCREEN_BOOT,
                .message = "Network formed, starting coordinator..."};
            xQueueSend(ui_event_queue, &event, 0);
            ESP_LOGI(TAG, "Network formation successful");
             vTaskDelay(SHORT_DELAY);
            uint8_t current_channel = esp_zb_get_current_channel();
            ESP_LOGW("SIGNAL FROMATION DEBUG", "Zigbee operating on channel: %d", current_channel);
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        }
        else
        {
            ui_event_t event = {
                .target_screen = SCREEN_BOOT,
                .message = "Network formation failed"};
            xQueueSend(ui_event_queue, &event, 0);
            ESP_LOGE(TAG, "Network formation failed");
      
        }
        break;
    }
    case ESP_ZB_BDB_SIGNAL_STEERING:
    {
        if (signal_struct->esp_err_status == ESP_OK)
        {
            ESP_LOGI(TAG, "Network steering completed");

            // Network is formed and ready, display the network key
            display_network_key();

            if (!nvs_check_for_paired_devices())
            {
                ui_event_t opening_event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "No saved devices found, opening network..."};
                xQueueSend(ui_event_queue, &opening_event, 0);
                ESP_LOGI(TAG, "No paired devices found, opening network...");
                 vTaskDelay(SHORT_DELAY);
                open_network(180); // Open for 3 minutes
            }
            else //no paired devices found
            {
                ui_event_t load_event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "loading devices from NVS..."};
                xQueueSend(ui_event_queue, &load_event, 0);
                ESP_LOGI(TAG, "Paired devices found, loading from nvs...");
                vTaskDelay(SHORT_DELAY);
                if (load_devices_from_nvs() == ESP_OK)
                {
                    if (trv_count == 0)
                    {
                        ESP_LOGW(TAG, "No TRVs found in NVS, opening network for pairing");
                        ui_event_t no_trv_event = {
                            .target_screen = SCREEN_BOOT,
                            .message = "No TRVs found"};
                        xQueueSend(ui_event_queue, &no_trv_event, 0);
                        vTaskDelay(SHORT_DELAY);

                        ui_event_t reopen_event = {
                            .target_screen = SCREEN_BOOT,
                            .message = PAIRING_MODE_UI_MESSAGE};
                        vTaskDelay(LONG_DELAY); // Give time for the message to be displayed
                        xQueueSend(ui_event_queue, &reopen_event, 0);
                        ESP_LOGI(TAG, "Network open, add devices...");

                        open_network(180); // Open for 3 minutes
                    }
                    else
                    {
                        ESP_LOGI(TAG, "TRVs found in NVS, proceeding to main screen");

                        ui_event_t loaded_event = {
                            .target_screen = SCREEN_BOOT,
                            .message = "Devices loaded from memory"};
                        xQueueSend(ui_event_queue, &loaded_event, 0);
                        ESP_LOGI(TAG, "Devices loaded from NVS including at least 1 trv");
                        vTaskDelay(SHORT_DELAY);
                        close_network(); // Close network after loading devices
                        ui_event_t close_event = {
                            .target_screen = SCREEN_BOOT,
                            .message = "Starting thermostat"};
                        xQueueSend(ui_event_queue, &close_event, 0);
                        vTaskDelay(SHORT_DELAY);       // Give time for the message to be displayed
                        ui_switch_screen(SCREEN_MAIN); // Switch to main screen
                        ESP_LOGI(TAG, "Network closed, returning to main screen");
                    }
                }
                else
                {
                    ui_event_t failed_event = {
                        .target_screen = SCREEN_BOOT,
                        .message = "No devices in memory"};
                    xQueueSend(ui_event_queue, &failed_event, 0);
                    ESP_LOGE(TAG, "Failed to load devices from NVS");
                     vTaskDelay(SHORT_DELAY);

                     ui_event_t reopen_event = {
                         .target_screen = SCREEN_BOOT,
                         .message = PAIRING_MODE_UI_MESSAGE};
                     vTaskDelay(pdMS_TO_TICKS(1500)); // Give time for the message to be displayed
                     xQueueSend(ui_event_queue, &reopen_event, 0);
                     ESP_LOGI(TAG, "Network open, add devices...");
                     open_network(180); // Open for 3 minutes
                }
                
            }
            break;
        }
    }

    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
    {
        ESP_LOGI(TAG, "Permit join status changed");
        if (network_open)
        {
            ui_event_t event = {
                .target_screen = SCREEN_BOOT,
                .message = PAIRING_MODE_UI_MESSAGE};
            xQueueSend(ui_event_queue, &event, 0);
        }
        break;
    }

    case ESP_ZB_ZDO_SIGNAL_LEAVE:
    {

        ui_event_t event = {
            .target_screen = SCREEN_BOOT,
            .message = "Device leaving network..."};
        xQueueSend(ui_event_queue, &event, portMAX_DELAY);
        ESP_LOGI(TAG, "Device leaving network");
        // Device will restart after this
        break;
    }

    case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION:
    {
        esp_zb_zdo_signal_leave_indication_params_t *leave_params =
            (esp_zb_zdo_signal_leave_indication_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        // find the device in our stored list
        for (uint8_t i = 0; i < stored_device_count; i++)
        {
            if (stored_devices[i].short_addr == leave_params->short_addr)
            {
                char message[64];
                snprintf(message, sizeof(message), "Device %s left network", stored_devices[i].name);
                ui_event_t event = {
                    .target_screen = SCREEN_BOOT,
                    .message = ""};
                strncpy(event.message, message, sizeof(event.message) - 1);
                xQueueSend(ui_event_queue, &event, 0);
                ESP_LOGI(TAG, "%s", message);
                break;
            }
        }
        // If the device is not in our list, just log it
        ui_event_t event = {
            .target_screen = SCREEN_BOOT,
            .message = "Device left network"};
        xQueueSend(ui_event_queue, &event, portMAX_DELAY);
        ESP_LOGI(TAG, "Device 0x%04x left network", leave_params->short_addr);
         vTaskDelay(SHORT_DELAY);
        break;
    }

    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
    {
        ESP_LOGI("SIGNAL HANDLER Device annce", "Device announcement received");
        dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        uint16_t new_addr = dev_annce_params->device_short_addr;
        esp_zb_ieee_addr_t new_ieee_addr;
        memcpy(new_ieee_addr, dev_annce_params->ieee_addr, sizeof(esp_zb_ieee_addr_t));

        if (device_exists(new_addr))
        {
            // find device name in our stored list
            for (uint8_t i = 0; i < stored_device_count; i++)
            {
                if (stored_devices[i].short_addr == new_addr)
                {
                    char message[64];
                    snprintf(message, sizeof(message), "Device %s rejoined network", stored_devices[i].name);
                    ui_event_t event = {
                        .target_screen = SCREEN_BOOT,
                        .message = ""};
                    strncpy(event.message, message, sizeof(event.message) - 1);
                    xQueueSend(ui_event_queue, &event, 0);
                    ESP_LOGI(TAG, "%s", message);
                     vTaskDelay(SHORT_DELAY);
                    
                    if(current_screen == SCREEN_BOOT && network_open){
                            ui_event_t reopen_event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "Network open, add devices...\nShort press button to close network"};
                    vTaskDelay(SHORT_DELAY); // Give time for the message to be displayed
                    xQueueSend(ui_event_queue, &reopen_event, 0);
                    }
                    break;
                }
            }
        }
        else
        {

            ESP_LOGI(TAG, "New device joining network (short: 0x%04x)", new_addr);

            esp_zb_zdo_match_desc_req_param_t cmd_req = {
                .dst_nwk_addr = new_addr,
                .addr_of_interest = new_addr};

            // Try to identify as window sensor first (simpler device)
            find_window_sensor(&cmd_req, find_window_sensor_cb, new_ieee_addr);

            // If that fails (no response after timeout), try as TRV
            vTaskDelay(SHORT_DELAY); // Give window sensor check time to complete
            // check to see if window sensor was found and stored
            if (!found_window_sensor)
            {
                ESP_LOGI(TAG, "Window sensor not found, trying TRV");
                // If window sensor not found, try to identify as TRV
                find_trv(&cmd_req, find_trv_cb, new_ieee_addr);
            }
            else
            {
                ESP_LOGI(TAG, "Window sensor found, skipping TRV search");
            }
        }
        break;
    }

    case ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED:
    {
        esp_zb_zdo_signal_device_authorized_params_t *auth_params =
            (esp_zb_zdo_signal_device_authorized_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGI(TAG, "Device authorized short address: 0x%04x", auth_params->short_addr);

        // Find the device in our stored list
        for (uint8_t i = 0; i < stored_device_count; i++)
        {
            if (stored_devices[i].short_addr == auth_params->short_addr)
            {
                char message[64];
                snprintf(message, sizeof(message), "Device %s authorized", stored_devices[i].name);
                ui_event_t event = {
                    .target_screen = SCREEN_BOOT,
                    .message = ""};
                strncpy(event.message, message, sizeof(event.message) - 1);
                xQueueSend(ui_event_queue, &event, 0);
                ESP_LOGI(TAG, "%s", message);
                break;
            }
        }

        break;
    }
    case ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED: // 0x12
        ESP_LOGI(TAG, "Device associated with network");
        break;

    case ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE: // 0x30
        ESP_LOGI(TAG, "Device update notification received");

        esp_zb_zdo_signal_device_update_params_t *update_params =
            (esp_zb_zdo_signal_device_update_params_t *)esp_zb_app_signal_get_params(p_sg_p);

        ESP_LOGI("SIGNAL HANDLER - device update", "Device update notification received from %s",get_device_name( update_params->short_addr));
        char status_message[64];
        uint16_t status=update_params->status;
        if(status == ESP_ZB_ZDO_STANDARD_DEV_SECURED_REJOIN){
            sprintf(status_message, "Device %s rejoined network ",get_device_name( update_params->short_addr));
        } 
        else if(status == ESP_ZB_ZDO_STANDARD_DEV_UNSECURED_JOIN){
            sprintf(status_message, "Device %s rejoined unsecured ",get_device_name( update_params->short_addr));
        } 
        else if(status == ESP_ZB_ZDO_STANDARD_DEV_LEFT){
            sprintf(status_message, "Device %s left network ",get_device_name( update_params->short_addr));
        } 
        else if(status == ESP_ZB_ZDO_STANDARD_DEV_TC_REJOIN){
            sprintf(status_message, "Device %s rejoined as TrustCentre ",get_device_name( update_params->short_addr));
        } 
        else {
            sprintf(status_message, "Device %s unknown status %x",get_device_name( update_params->short_addr),status);
        }

        ESP_LOGI("SIGNAL HANDLER-device update", "%s", status_message);
          ui_event_t event = {
                    .target_screen = SCREEN_BOOT,
                    .message = ""};
                strncpy(event.message, status_message, sizeof(event.message) - 1);
                xQueueSend(ui_event_queue, &event, 0);
        
        break;

    case ESP_ZB_NLME_STATUS_INDICATION: // 0x32
    {
        esp_zb_zdo_signal_nwk_status_indication_params_t *status_params =
            (esp_zb_zdo_signal_nwk_status_indication_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        if (device_exists(status_params->network_addr))
        {
            for (uint8_t i = 0; i < stored_device_count; i++)
            {
                if (stored_devices[i].short_addr == status_params->network_addr)
                {
                    char status[64];
                    if (status_params->status== ESP_ZB_NWK_COMMAND_STATUS_INDIRECT_TRANSACTION_EXPIRY)
                    {
                        snprintf(status, sizeof(status), "Indirect transaction expiry");
                    }
                    else if (status_params->status == ESP_ZB_NWK_COMMAND_STATUS_BAD_KEY_SEQUENCE_NUMBER)
                    {
                        snprintf(status, sizeof(status), "Bad key sequence number");
                    }
                   
                    else
                    {
                        snprintf(status, sizeof(status), "Unknown status: %x", status_params->status);
                    }
                    ESP_LOGI("ZIGBEE SIGNAL HANDLER", "NETWORK ID CONFLICT received from: %s, status: %x", stored_devices[i].name, status_params->status);
                    char message[256];
                    snprintf(message, sizeof(message), "Device status indication from %s, status %s", stored_devices[i].name,status);
                    ui_display_message(message);
                    break;
                }
            }
        }
        else
        {
            // If the device is not in our list, just log it

            ESP_LOGI(TAG, "Device status indication received from: 0x%04x", status_params->network_addr);
        }

   

        // Only handle bad key sequence, ignore other statuses
        if (status_params->status == ESP_ZB_NWK_COMMAND_STATUS_BAD_KEY_SEQUENCE_NUMBER)
        {
            ESP_LOGW(TAG, "Bad key sequence detected, sending leave request");
            esp_zb_zdo_mgmt_leave_req_param_t leave_req = {
                .dst_nwk_addr = status_params->network_addr,
                .rejoin = 1, // Allow rejoin
                .remove_children = 0};
            esp_zb_zdo_device_leave_req(&leave_req, NULL, NULL);
        }
        break;
    }
    case ESP_ZB_ZDO_DEVICE_UNAVAILABLE: // 0x3c
    {
        esp_zb_zdo_device_unavailable_params_t *unavailable_params =
            (esp_zb_zdo_device_unavailable_params_t *)esp_zb_app_signal_get_params(p_sg_p);

        ESP_LOGW(TAG, "Device unavailable: Short address: 0x%04x", unavailable_params->short_addr);

        // Try to identify which device became unavailable
        for (uint8_t i = 0; i < stored_device_count; i++)
        {
            if (stored_devices[i].short_addr == unavailable_params->short_addr)
            {
                ESP_LOGW(TAG, "Lost connection to device: %s", stored_devices[i].name);
                break;
            }
        }
    }
    break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT: // 0x06
        ESP_LOGI(TAG, "Device reboot signal received");
        break;

    default:
        ESP_LOGW(TAG, "Unhandled Zigbee signal: %d (0x%x)", sig_type, sig_type);
        break;
    }
}
void reset_device(void){

if(stored_device_count >0){

    for(int i=0; i<stored_device_count; i++){
        ESP_LOGI(TAG, "Sending leave request to device %s ieee address %2x", stored_devices[i].name, stored_devices[i].ieee_addr);
        unpair_device(stored_devices[i].short_addr);
    }
} else {
    ESP_LOGI(TAG, "No stored devices to unpair");
    ui_event_t ui_event = {
        .target_screen = current_screen,
        .message = "No stored devices to unpair..."};
    xQueueSend(ui_event_queue, &ui_event, 0);
    vTaskDelay(SHORT_DELAY);
    restart();
}
 
}
void unpair_device(uint16_t short_addr)
{
    uint8_t unpair_attempts=get_device_by_short_addr(short_addr)->unpair_attempts;
    zigbee_device_t *device = get_device_by_short_addr(short_addr);
    device->unpair_attempts++;

  ESP_LOGI(TAG, "attempt %d of %d to unpair device %s...",unpair_attempts+1,MAX_UNPAIR_ATTEMPTS, get_device_name(short_addr));
     ui_event_t ui_event = {
            .target_screen = current_screen,
            .message = ""};
        snprintf(ui_event.message, sizeof(ui_event.message), "Device %s left network", get_device_name(short_addr));
        xQueueSend(ui_event_queue, &ui_event, 0);
        vTaskDelay(SHORT_DELAY);
    esp_zb_zdo_mgmt_leave_req_param_t leave_req = {
        .dst_nwk_addr = short_addr,
        .rejoin = 0, // Do not allow rejoin
        .remove_children = 0}; // devices hould have no children
    esp_zb_zdo_device_leave_req(&leave_req, reset_device_cb, (void *)short_addr);
}

void reset_device_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx)
{
    uint16_t short_address=(uint16_t)(uintptr_t)user_ctx;
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS)
    {
        ESP_LOGI(TAG, "Device leave request sent successfully for %s", get_device_name(short_address));
        devices_left++;

        ui_event_t ui_event = {
            .target_screen = current_screen,
            .message = ""};
        snprintf(ui_event.message, sizeof(ui_event.message), "Device %s left network", get_device_name(short_address));
        xQueueSend(ui_event_queue, &ui_event, 0);
        vTaskDelay(SHORT_DELAY);

        if(devices_left == stored_device_count){
            ESP_LOGI(TAG, "All devices have been sent leave request");
            ESP_LOGI(TAG, "Clearing device storage...");
                    
        ui_event_t ui_event = {
            .target_screen = current_screen,
            .message = "All devices successfully unpaired..."};
        xQueueSend(ui_event_queue, &ui_event, 0);
        vTaskDelay(SHORT_DELAY);

        strcpy(ui_event.message , "Clearing Device Storage...");
        xQueueSend(ui_event_queue, &ui_event, 0);
        clear_all_nvs();
        vTaskDelay(SHORT_DELAY);

        ESP_LOGI(TAG, "Restarting device in 2...");
        strcpy(ui_event.message , "Restarting device in 2...");
        xQueueSend(ui_event_queue, &ui_event, 0);
        vTaskDelay(SHORT_DELAY);

        ESP_LOGI(TAG, "Restarting device in 1...");
        strcpy(ui_event.message , "Restarting device in 1...");
        xQueueSend(ui_event_queue, &ui_event, 0);
        vTaskDelay(SHORT_DELAY);

        esp_restart();
            esp_restart();
        }
    }
    else
    {
        ESP_LOGW(TAG, "Device leave request for device %s failed with status: 0x%x", (char *) get_device_name(short_address),zdo_status);
      
        ui_event_t ui_event = {
            .target_screen = current_screen,
            .message = ""};
         snprintf(ui_event.message, sizeof(ui_event.message), "Device leave request failed for %s...", get_device_name(short_address));
        xQueueSend(ui_event_queue, &ui_event, 0);
        vTaskDelay(SHORT_DELAY);

        zigbee_device_t *device = get_device_by_short_addr(short_address);
        if(device->unpair_attempts < MAX_UNPAIR_ATTEMPTS){
            ESP_LOGI(TAG, "Retrying unpair for device %s...", get_device_name(short_address));
            unpair_device(device->short_addr);
        } else {
            ESP_LOGI(TAG, "Max attempts reached for device %s, skipping...", get_device_name(short_address));
            devices_left++;
        }

        if(devices_left == stored_device_count){
            restart();
        }
    }
}

void restart(void){
       ESP_LOGI(TAG, "Clearing device storage...");
        ui_event_t ui_event = {
            .target_screen = current_screen,
            .message = ""};
        strcpy(ui_event.message , "Clearing Device Storage...");
        xQueueSend(ui_event_queue, &ui_event, 0);
        vTaskDelay(SHORT_DELAY);
        //TODO handle error
        clear_all_nvs();
        strcpy(ui_event.message , "Restarting device...");
        xQueueSend(ui_event_queue, &ui_event, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
}
zigbee_device_t *get_device_by_short_addr(uint16_t short_addr)
{
    for (int i = 0; i < stored_device_count; i++)
    {
        if (stored_devices[i].short_addr == short_addr)
        {
            return &stored_devices[i];
        }
    }
    return NULL;
}