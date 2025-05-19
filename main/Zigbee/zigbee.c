#include "nvs.h"
#include "nvs_flash.h"
#include "Zigbee/zigbee.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "zcl/esp_zigbee_zcl_ias_zone.h" // Added for IAS Zone specific definitions
#include "esp_timer.h"
#include "../LVGL_UI/ui_screens.h"
#include "../LVGL_UI/ui_events.h"
#include "../Helpers/helpers.h"

#define NVS_NAMESPACE "zigbee"
#define MAX_DEVICE_NAME_LENGTH 32
#define NVS_COMMIT_TIMEOUT_MS 1000
#define REJOIN_THRESHOLD_MS 5000

#define NVS_KEY_DEVICE_COUNT "dev_count"
#define NVS_KEY_DEVICE_BASE "device_"
#define MAX_TRV_DEVICES 2
#define MAX_WINDOW_SENSORS 3
#define MAX_DEVICE_NAME_LENGTH 32

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct trv_device_params_s
{
    esp_zb_ieee_addr_t ieee_addr;
    uint8_t endpoint;
    uint16_t short_addr;
} trv_device_params_t;

static trv_device_params_t trv;

static const char *TAG = "ZIGBEE";

// Define the global array
zigbee_device_t stored_devices[MAX_TRV_DEVICES + MAX_WINDOW_SENSORS];

uint8_t stored_device_count = 0;
static uint8_t trv_count = 0;
static uint8_t window_sensor_count = 0;
bool network_open = false;
static uint8_t last_config_tsn = 0; // tracking the last transaction sequence number for config report command
static bool found_window_sensor = false;
static bool found_trv = false;
extern QueueHandle_t ui_event_queue;

static bool device_exists(uint16_t short_addr)
{
    int64_t now = esp_timer_get_time() / 1000;

    for (uint8_t i = 0; i < stored_device_count; i++)
    {
        if (stored_devices[i].short_addr == short_addr)
        {
            // Check if device was seen recently
            if ((now - stored_devices[i].last_seen) > REJOIN_THRESHOLD_MS)
            {
                ESP_LOGI(TAG, "Device 0x%04x expired, treating as new join", short_addr);
                return false;
            }
            return true;
        }
    }
    return false;
}

esp_err_t save_device_to_nvs(zigbee_device_t *device)
{
    static int64_t last_save = 0;
    int64_t now = esp_timer_get_time() / 1000; // Convert to ms

    // First check if device already exists
    if (device_exists(device->short_addr))
    {
        // Update last seen time for existing device
        for (uint8_t i = 0; i < stored_device_count; i++)
        {
            if (stored_devices[i].short_addr == device->short_addr)
            {
                stored_devices[i].last_seen = now;
                ESP_LOGI(TAG, "Updated last seen time for device 0x%04x", device->short_addr);
                break;
            }
        }
        return ESP_OK;
    }

    // Prevent rapid successive writes to NVS
    if ((now - last_save) < NVS_COMMIT_TIMEOUT_MS)
    {
        ESP_LOGW(TAG, "Too soon to save to NVS, skipping");
        return ESP_OK;
    }

    if (stored_device_count >= (MAX_TRV_DEVICES + MAX_WINDOW_SENSORS))
    {
        ESP_LOGE(TAG, "Maximum device limit reached");
        return ESP_ERR_NO_MEM;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Save new device count
    err = nvs_set_u8(handle, NVS_KEY_DEVICE_COUNT, stored_device_count + 1);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error saving device count: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Create unique key for this device
    char key[16];
    snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEVICE_BASE, stored_device_count);

    // Save device data
    err = nvs_set_blob(handle, key, device, sizeof(zigbee_device_t));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error saving device data: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK)
    {
        device->last_seen = now;
        stored_devices[stored_device_count] = *device;
        stored_device_count++;
        last_save = now;
        ESP_LOGI(TAG, "Device saved successfully: %s", device->name);
    }

    return err;
}

esp_err_t save_devices_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;

    // Save device count
    err = nvs_set_u8(handle, NVS_KEY_DEVICE_COUNT, stored_device_count);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return err;
    }

    // Save each device
    for (uint8_t i = 0; i < stored_device_count; i++)
    {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEVICE_BASE, i);
        err = nvs_set_blob(handle, key, &stored_devices[i], sizeof(zigbee_device_t));
        if (err != ESP_OK)
        {
            nvs_close(handle);
            return err;
        }
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t clear_all_nvs(void)
{
    // Clear default NVS partition
    nvs_flash_erase();
    nvs_flash_init();

    // Clear Zigbee storage partition
    const esp_partition_t *zb_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "zb_storage");
    if (zb_partition)
    {
        esp_partition_erase_range(zb_partition, 0, zb_partition->size);
    }

    // Clear Zigbee factory partition
    const esp_partition_t *zb_fct_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "zb_fct");
    if (zb_fct_partition)
    {
        esp_partition_erase_range(zb_fct_partition, 0, zb_fct_partition->size);
    }
    trv_count = 0;
    window_sensor_count = 0;
    stored_device_count = 0;
    return ESP_OK;
}

bool nvs_check_for_paired_devices(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return false;
    }

    uint8_t count = 0;
    err = nvs_get_u8(handle, NVS_KEY_DEVICE_COUNT, &count);
    nvs_close(handle);

    return (err == ESP_OK && count > 0);
}

esp_err_t load_devices_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
        return err;

    // Load device count
    err = nvs_get_u8(handle, NVS_KEY_DEVICE_COUNT, &stored_device_count);
    if (err != ESP_OK)
    {
        stored_device_count = 0;
        nvs_close(handle);
        return err;
    }

    // Load each device
    for (uint8_t i = 0; i < stored_device_count; i++)
    {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEVICE_BASE, i);
        size_t size = sizeof(zigbee_device_t);
        err = nvs_get_blob(handle, key, &stored_devices[i], &size);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to load device %d", i);
            continue;
        }
        ESP_LOGI(TAG, "Loaded device: %s (0x%04x)",
                 stored_devices[i].name, stored_devices[i].short_addr);
    }

    nvs_close(handle);
    return ESP_OK;
}

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
        if (bind_req->req_dst_addr == esp_zb_get_short_address())
        {
            ESP_LOGI(TAG, "Trv bound to controller from address(0x%x) on trv endpoint(%d)",
                     trv.short_addr, trv.endpoint);

            /* Read peer Manufacture Name & Model Identifier */
            esp_zb_zcl_read_attr_cmd_t read_req = {0};
            read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
            read_req.zcl_basic_cmd.src_endpoint = HA_THERMOSTAT_ENDPOINT;
            read_req.zcl_basic_cmd.dst_endpoint = trv.endpoint;
            read_req.zcl_basic_cmd.dst_addr_u.addr_short = trv.short_addr;
            read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_BASIC;

            uint16_t attributes[] = {
                ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
            };
            read_req.attr_number = ARRAY_LENGTH(attributes);
            read_req.attr_field = attributes;

            esp_zb_zcl_read_attr_cmd_req(&read_req);
        }
        if (bind_req->req_dst_addr == trv.short_addr)
        {
            ESP_LOGI(TAG, "Controller bound to trv from sensor address(0x%x) on sensor endpoint(%d)",
                     trv.short_addr, trv.endpoint);
        }
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

        // Store device info with correct address
        trv_device_params_t *sensor = (trv_device_params_t *)user_ctx;
        if (!sensor)
        {
            ESP_LOGE(TAG, "Invalid user context");
            return;
        }
        sensor->endpoint = peer_endpoint;
        sensor->short_addr = peer_addr;
        esp_zb_ieee_address_by_short(sensor->short_addr, sensor->ieee_addr);

        /* 1. Send binding request to the sensor */
        esp_zb_zdo_bind_req_param_t *bind_req = (esp_zb_zdo_bind_req_param_t *)calloc(1, sizeof(esp_zb_zdo_bind_req_param_t));

        bind_req->req_dst_addr = peer_addr;

        // /* populate the src information of the binding */
        // memcpy(bind_req->src_address, sensor->ieee_addr, sizeof(esp_zb_ieee_addr_t));
        // bind_req->src_endp = peer_endpoint;
        // bind_req->cluster_id = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;

        // /* populate the dst information of the binding */
        // bind_req->dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
        // esp_zb_get_long_address(bind_req->dst_address_u.addr_long);
        // bind_req->dst_endp = HA_THERMOSTAT_ENDPOINT;

        // ESP_LOGI(TAG, "Binding TRV to coordinator (receive data)");
        // esp_zb_zdo_device_bind_req(bind_req, bind_trv_cb, bind_req);

        /* 2. Send binding request to self */
        bind_req = (esp_zb_zdo_bind_req_param_t *)calloc(1, sizeof(esp_zb_zdo_bind_req_param_t));

        bind_req->req_dst_addr = esp_zb_get_short_address();

        /* populate the src information of the binding */
        esp_zb_get_long_address(bind_req->src_address);
        bind_req->src_endp = HA_THERMOSTAT_ENDPOINT;
        bind_req->cluster_id = ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT;

        /* populate the dst information of the binding */
        bind_req->dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
        memcpy(bind_req->dst_address_u.addr_long, sensor->ieee_addr, sizeof(esp_zb_ieee_addr_t));
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
    esp_zb_zdo_match_cluster(param, callback, user_ctx ? user_ctx : (void *)&trv);
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

        // Configure reporting command
        // First, write the CIE address (identify this coordinator as the CIE)
        //     esp_zb_ieee_addr_t coordinator_addr;

        //     // Get the coordinator's long address
        //     esp_zb_get_long_address(coordinator_addr);
        //     ESP_LOGI(TAG, "Coordinator address: 0x%02x%02x%02x%02x%02x%02x%02x%02x",
        //              coordinator_addr[7], coordinator_addr[6], coordinator_addr[5], coordinator_addr[4],
        //              coordinator_addr[3], coordinator_addr[2], coordinator_addr[1], coordinator_addr[0]);

        //    // Create a static attribute structure to ensure memory validity
        //     static esp_zb_zcl_attribute_t attr = {
        //         .id = ESP_ZB_ZCL_ATTR_IAS_ZONE_IAS_CIE_ADDRESS_ID,
        //         .data.type = ESP_ZB_ZCL_ATTR_TYPE_IEEE_ADDR,
        //         .data.size = sizeof(esp_zb_ieee_addr_t),
        //     };
        //     // Set the value pointer to our coordinator address
        //     attr.data.value = coordinator_addr;

        //     esp_zb_zcl_write_attr_cmd_t write_cmd = {
        //         .zcl_basic_cmd = {
        //             .dst_addr_u.addr_short = bind_req->req_dst_addr,
        //             .dst_endpoint = bind_req->dst_endp,
        //             .src_endpoint = bind_req->src_endp,
        //         },
        //         .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        //         .clusterID = ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE,
        //         .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        //         .attr_number = 1,
        //         .attr_field = &attr
        //     };
        //    esp_err_t err = esp_zb_zcl_write_attr_cmd_req(&write_cmd);
        //     if (err != ESP_OK) {
        //         ESP_LOGE(TAG, "Failed to write CIE address: %s", esp_err_to_name(err));
        //     }

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

    // ESP_LOGI(TAG, "Window sensor callback triggered:");
    // ESP_LOGI(TAG, "  Status: 0x%x", zdo_status);
    // ESP_LOGI(TAG, "  Address: 0x%04x", peer_addr);
    // ESP_LOGI(TAG, "  Endpoint: %d", peer_endpoint);

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
            uint8_t current_channel = esp_zb_get_current_channel();
            ESP_LOGI("debug sniffer", "Zigbee operating on channel: %d", current_channel);
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
                open_network(180); // Open for 3 minutes
            }
            else
            {
                ui_event_t load_event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "loading devices from NVS..."};
                xQueueSend(ui_event_queue, &load_event, 0);
                ESP_LOGI(TAG, "Paired devices found, loading from nvs...");
                if (load_devices_from_nvs() == ESP_OK)
                {
                    ui_event_t loaded_event = {
                        .target_screen = SCREEN_BOOT,
                        .message = "Devices loaded from NVS"};
                    xQueueSend(ui_event_queue, &loaded_event, 0);
                    ESP_LOGI(TAG, "Devices loaded from NVS");
                }
                else
                {
                    ui_event_t failed_event = {
                        .target_screen = SCREEN_BOOT,
                        .message = "Failed to load devices from NVS"};
                    xQueueSend(ui_event_queue, &failed_event, 0);
                    ESP_LOGE(TAG, "Failed to load devices from NVS");
                }
                ui_event_t reopen_event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "Network open, add devices...then short press button to close network"};
                vTaskDelay(pdMS_TO_TICKS(1500)); // Give time for the message to be displayed
                xQueueSend(ui_event_queue, &reopen_event, 0);
                ESP_LOGI(TAG, "Network open, add devices...");
                open_network(180); // Open for 3 minutes
            }
        }
        break;
    }

    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
    {
        ESP_LOGI(TAG, "Permit join status changed");
        if (network_open)
        {
            ui_event_t event = {
                .target_screen = SCREEN_BOOT,
                .message = "Zigbee Network open - Add devices then short press button when done"};
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
        break;
    }

    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
    {
        dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        uint16_t new_addr = dev_annce_params->device_short_addr;

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
                    
                    if(current_screen == SCREEN_BOOT && network_open){
                            ui_event_t reopen_event = {
                    .target_screen = SCREEN_BOOT,
                    .message = "Network open, add devices...\nShort press button to close network"};
                    vTaskDelay(pdMS_TO_TICKS(1500)); // Give time for the message to be displayed
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
            find_window_sensor(&cmd_req, find_window_sensor_cb, NULL);

            // If that fails (no response after timeout), try as TRV
            vTaskDelay(pdMS_TO_TICKS(1000)); // Give window sensor check time to complete
            // check to see if window sensor was found and stored
            if (!found_window_sensor)
            {
                ESP_LOGI(TAG, "Window sensor not found, trying TRV");
                // If window sensor not found, try to identify as TRV
                find_trv(&cmd_req, find_trv_cb, NULL);
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
        // ESP_LOGI(TAG, "Device authorised long address: 0x%02x%02x%02x%02x%02x%02x%02x%02x",
        //          auth_params->long_addr[7], auth_params->long_addr[6], auth_params->long_addr[5],
        //          auth_params->long_addr[4], auth_params->long_addr[3], auth_params->long_addr[2],
        //          auth_params->long_addr[1], auth_params->long_addr[0]);
        // ESP_LOGI(TAG, "Auth type: %d", auth_params->authorization_type);
        // ESP_LOGI(TAG, "Auth status: 0x%04x", auth_params->authorization_status);
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
        ESP_LOGI("SIGNAL HANDLER", "Device update notification received from %d",get_device_name( update_params->short_addr));
        // ESP_LOGI(TAG, "Device short address: 0x%04x", update_params->short_addr);
        // ESP_LOGI(TAG, "Device long address: 0x%02x%02x%02x%02x%02x%02x%02x%02x",
        //          update_params->long_addr[7], update_params->long_addr[6], update_params->long_addr[5],
        //          update_params->long_addr[4], update_params->long_addr[3], update_params->long_addr[2],
        //          update_params->long_addr[1], update_params->long_addr[0]);
        // ESP_LOGI(TAG, "Device status: %d", update_params->status);
        // ESP_LOGI(TAG, "Device trust centre action: %d", update_params->tc_action);
        // ESP_LOGI(TAG, "Device parent short: %d", update_params->parent_short);
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

void display_network_key(void)
{
    uint8_t network_key[16];
    esp_err_t status = esp_zb_secur_primary_network_key_get(network_key);

    if (status == ESP_OK)
    {
        ESP_LOGI(TAG, "Zigbee Network Key: ");
        for (int i = 0; i < 16; i++)
        {
            printf("%02x", network_key[i]);
            if (i < 15)
                printf(":");
        }
        printf("\n");
    }
    else
    {
        ESP_LOGW(TAG, "Failed to get network key, error: %s", esp_err_to_name(status));
        ESP_LOGW(TAG, "Note: Network key can only be obtained after the device has joined the network");
    }
}

void ui_display_message(const char *message)
{
    ui_event_t event = {
        .target_screen = SCREEN_BOOT,
        .message = ""};
    strncpy(event.message, message, sizeof(event.message) - 1);
    xQueueSend(ui_event_queue, &event, 0);
}