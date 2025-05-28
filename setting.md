static void zigbee_task(void *pvParameters)
{
    // Setup a semaphore to block until settings are configured
    static SemaphoreHandle_t settings_complete = NULL;
    settings_complete = xSemaphoreCreateBinary();
    
    // Set default values
    uint32_t channel_mask = ESP_ZB_PRIMARY_CHANNEL_MASK;
    bool settings_found = false;
    
    // Try to read settings from NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("zigbee", NVS_READONLY, &nvs_handle);
    
    if (err == ESP_OK) {
        // Try to read channel mask
        err = nvs_get_u32(nvs_handle, "channel_mask", &channel_mask);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Channel mask loaded from NVS: 0x%08x", channel_mask);
            settings_found = true;
            
            // Read other settings
            // Example: read other global settings
            uint16_t temp;
            if (nvs_get_u16(nvs_handle, "target_temp", &temp) == ESP_OK) {
                g_target_temp = temp;
                ESP_LOGI(TAG, "Target temperature loaded: %.2f°C", g_target_temp/100.0f);
            }
            
            if (nvs_get_u16(nvs_handle, "min_temp", &temp) == ESP_OK) {
                g_min_temp = temp;
                ESP_LOGI(TAG, "Min temperature loaded: %.2f°C", g_min_temp/100.0f);
            }
            
            if (nvs_get_u16(nvs_handle, "max_temp", &temp) == ESP_OK) {
                g_max_temp = temp; 
                ESP_LOGI(TAG, "Max temperature loaded: %.2f°C", g_max_temp/100.0f);
            }
        } else {
            ESP_LOGW(TAG, "No channel mask in NVS, need configuration");
        }
        nvs_close(nvs_handle);
    }
    
    // If settings not found, enter settings mode
    if (!settings_found) {
        // Global flag to tell other parts of the app we're in initial setup
        bool initial_setup_mode = true;
        
        // Display settings screen and wait for user to configure
        ui_event_t event = {
            .target_screen = SCREEN_SETTINGS,
            .message = "Initial Setup - Please configure network settings"
        };
        xQueueSend(ui_event_queue, &event, portMAX_DELAY);
        
        // Register callback for settings completion
        settings_register_callback(settings_complete_cb, settings_complete);
        
        // Block until settings are configured
        ESP_LOGI(TAG, "Waiting for settings configuration...");
        xSemaphoreTake(settings_complete, portMAX_DELAY);
        ESP_LOGI(TAG, "Settings configured, continuing with initialization");
        
        // Reload settings from NVS now that they've been saved
        err = nvs_open("zigbee", NVS_READONLY, &nvs_handle);
        if (err == ESP_OK) {
            if (nvs_get_u32(nvs_handle, "channel_mask", &channel_mask) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to read channel mask after config, using default");
            }
            nvs_close(nvs_handle);
        }
    }

    // Now initialize Zigbee with the settings (either loaded or newly configured)
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    /* Create customized thermostat endpoint */
    esp_zb_thermostat_cfg_t thermostat_cfg = ESP_ZB_DEFAULT_THERMOSTAT_CONFIG();
    esp_zb_ep_list_t *esp_zb_endpoints = create_endpoints(&thermostat_cfg);

    /* Register the device */
    esp_zb_device_register(esp_zb_endpoints);

    // Set channel mask from settings
    esp_zb_set_primary_network_channel_set(channel_mask);

    esp_zb_core_action_handler_register(zb_action_handler);

    // Start Zigbee stack with autostart
    ESP_ERROR_CHECK(esp_zb_start(false));

    // Enter Zigbee main loop
    esp_zb_stack_main_loop();
}