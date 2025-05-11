signal received (*signal_struct)
get a pointer to the signal type
i.e. uint32 *p_sg_p = signal_struct->p_app_signal
then use this pointer to get the actual signal type itself i.e.
esp_zb_app_signal_type_t sig_type= *p_sg_p

initialise device announce params by creating a pointer to a announce params type i.e. esp_zb_zdo_signal_device_annce_params_t *dev_annce_params=NULL;

switch the signal to interrogate its  type
we are after the announce params
check if we already have the device - we have a struct zigbee_device_t that contains its type, short_addr, endpoint, name, and last_seen. we have enums to reference the device type called DEVICE_TYPE_TRV and DEVICE_TYPE_WINDOW_SENSOR and these are device_type_t

if not we pass the announce params into identify_device_type(*params) which returns a device type (device_type_t)
The `params->capability` field in the `esp_zb_zdo_signal_device_annce_params_t` structure is a **bitmask** that represents the capabilities of a Zigbee device. It provides information about the device's role, power source, and other features.

---

### **Structure of `capability`**
The `capability` field is typically an 8-bit value where each bit represents a specific capability of the device. Here's a breakdown of the bits:

| **Bit** | **Hex Mask** | **Description**                                                                 |
|---------|--------------|---------------------------------------------------------------------------------|
| 7       | `0x80`       | Device is capable of acting as a coordinator.                                  |
| 6       | `0x40`       | Device is capable of acting as a router.                                       |
| 5       | `0x20`       | Reserved (usually unused).                                                     |
| 4       | `0x10`       | Device is mains-powered (1) or battery-powered (0).                            |
| 3       | `0x08`       | Device can act as a receiver during idle periods (1) or not (0).               |
| 2       | `0x04`       | Reserved (usually unused).                                                     |
| 1       | `0x02`       | Device is capable of acting as a full-function device (FFD) or reduced-function device (RFD). |
| 0       | `0x01`       | Device is capable of acting as an end device.                                  |

---

### **How to Use `params->capability`**
You can use the `capability` field to determine the role and features of the device. This is done by performing bitwise operations to check specific bits.

#### **Example: Logging the Capabilities**
```c
ESP_LOGI(TAG, "Device capabilities: 0x%02x", params->capability);

ESP_LOGI(TAG, "Capabilities breakdown:");
ESP_LOGI(TAG, "Bit 7 (Coordinator): %d", (params->capability & 0x80) ? 1 : 0);
ESP_LOGI(TAG, "Bit 6 (Router): %d", (params->capability & 0x40) ? 1 : 0);
ESP_LOGI(TAG, "Bit 4 (Mains-powered): %d", (params->capability & 0x10) ? 1 : 0);
ESP_LOGI(TAG, "Bit 3 (Receiver during idle): %d", (params->capability & 0x08) ? 1 : 0);
ESP_LOGI(TAG, "Bit 1 (Full-function device): %d", (params->capability & 0x02) ? 1 : 0);
ESP_LOGI(TAG, "Bit 0 (End device): %d", (params->capability & 0x01) ? 1 : 0);
```

#### **Example Output**
For a device with `params->capability = 0x52`:
```
Device capabilities: 0x52
Capabilities breakdown:
Bit 7 (Coordinator): 0
Bit 6 (Router): 1
Bit 4 (Mains-powered): 1
Bit 3 (Receiver during idle): 0
Bit 1 (Full-function device): 1
Bit 0 (End device): 0
```

---

### **Using `capability` to Make Decisions**
You can use the `capability` field to make decisions about how to handle the device. For example:

#### **Check if the Device is Battery-Powered**
```c
if (!(params->capability & 0x10)) {
    ESP_LOGI(TAG, "Device is battery-powered");
}
```

#### **Check if the Device is a Full-Function Device (FFD)**
```c
if (params->capability & 0x02) {
    ESP_LOGI(TAG, "Device is a full-function device (FFD)");
} else {
    ESP_LOGI(TAG, "Device is a reduced-function device (RFD)");
}
```

#### **Check if the Device is a Router**
```c
if (params->capability & 0x40) {
    ESP_LOGI(TAG, "Device is capable of acting as a router");
}
```

---

### **Fallback Identification**
If the MAC address prefix is not recognized, you can use the `capability` field as a fallback to infer the device type. For example:
```c
if ((params->capability & 0x40) && (params->capability & 0x10)) {
    ESP_LOGI(TAG, "Device is likely a mains-powered router");
    return DEVICE_TYPE_ROUTER;
} else if (!(params->capability & 0x10)) {
    ESP_LOGI(TAG, "Device is likely a battery-powered end device");
    return DEVICE_TYPE_SENSOR;
}
```

---

### **Summary**
- The `params->capability` field is a bitmask that provides information about the device's role and features.
- You can use bitwise operations to extract specific capabilities.
- This field is useful for identifying the device type, especially when the MAC address prefix is not sufficient.
 ###Match description request ###
we need to send a match description request to find if it has the required cluster enabled
esp_zb_zdo_match_desc_req_param_t match_req = {
    .dst_nwk_addr = device_short_addr,  // Short address of the device
    .addr_of_interest = device_short_addr,  // Same as destination
    .cluster_list = {0x0500},  // IAS Zone Cluster
    .cluster_count = 1,  // Number of clusters to match
};

so...
````
 esp_zb_zdo_match_desc_req_param_t match_req = {
    .dst_nwk_addr = device_short_addr,  // Short address of the device
    .addr_of_interest = device_short_addr,  // Same as destination
    .cluster_list = {ZB_ZCL_CLUSTER_ID_IAS_ZONE},  // IAS Zone Cluster
    .cluster_count = 1,  // Number of clusters to match
};

// Send the Match Descriptor Request
esp_zb_zdo_device_match_desc_req(&match_req, match_desc_cb, NULL);
````
Yes, for your **window sensor** to be found, bound, and used in your application, it must support the **IAS Zone Cluster (`ZB_ZCL_CLUSTER_ID_IAS_ZONE`, 0x0500)**. You need to send a **Match Descriptor Request** to the device to check if it supports this cluster. If the device supports the IAS Zone Cluster, you can proceed to bind it and use it in your application.

Here’s how you can achieve this:

---

### **Steps to Match the IAS Zone Cluster**

#### **1. Prepare the Match Descriptor Request**
The `esp_zb_zdo_match_desc_req_param_t` structure is used to configure the Match Descriptor Request. You need to:
- Specify the **short address** of the device (`dst_nwk_addr`).
- Specify the **cluster list** to match against (in this case, the IAS Zone Cluster `0x0500`).

#### **Code Example: Match Descriptor Request**
```c
esp_zb_zdo_match_desc_req_param_t match_req = {
    .dst_nwk_addr = device_short_addr,  // Short address of the device
    .addr_of_interest = device_short_addr,  // Same as destination
    .cluster_list = {ZB_ZCL_CLUSTER_ID_IAS_ZONE},  // IAS Zone Cluster
    .cluster_count = 1,  // Number of clusters to match
};

// Send the Match Descriptor Request
esp_zb_zdo_device_match_desc_req(&match_req, match_desc_cb, NULL);
```

---

#### **2. Handle the Match Descriptor Response**
The response to the Match Descriptor Request is handled in a callback function. If the device supports the IAS Zone Cluster, the callback will indicate success, and you can proceed to bind the device.

#### **Callback Example**
```c
static void match_desc_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Device 0x%04x supports the IAS Zone Cluster on endpoint %d", addr, endpoint);
        // Proceed to bind the device
        bind_device(addr, endpoint);
    } else {
        ESP_LOGW(TAG, "Device 0x%04x does not support the IAS Zone Cluster", addr);
    }
}
```

---

### **3. Bind the Device**
Once you confirm that the device supports the IAS Zone Cluster, you can bind it to the coordinator. Binding allows the device to send reports (e.g., open/closed state) to the coordinator.

#### **Code Example: Binding the Device**
```c
static void bind_device(uint16_t addr, uint8_t endpoint) {
    esp_zb_zdo_bind_req_param_t bind_req = {
        .src_addr = addr,  // Short address of the device
        .src_endpoint = endpoint,  // Endpoint of the device
        .cluster_id = ZB_ZCL_CLUSTER_ID_IAS_ZONE,  // IAS Zone Cluster
        .dst_addr_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT,
        .dst_address_u.ieee_addr = esp_zb_get_long_address(),  // Coordinator's IEEE address
        .dst_endpoint = 1,  // Coordinator's endpoint
    };

    esp_zb_zdo_device_bind_req(&bind_req, bind_cb, NULL);
}
```

#### **Binding Callback**
The binding request will invoke a callback to confirm whether the binding was successful:
```c
static void bind_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Binding successful");
    } else {
        ESP_LOGW(TAG, "Binding failed");
    }
}
```

---

### **4. Use the Device**
After binding, the window sensor will send reports to the coordinator whenever its state changes (e.g., open/closed). You can handle these reports in your application by implementing a Zigbee cluster handler for the IAS Zone Cluster.

---

### **Summary**
1. **Send a Match Descriptor Request**:
   - Use `esp_zb_zdo_device_match_desc_req` to check if the device supports the IAS Zone Cluster (`0x0500`).

2. **Handle the Response**:
   - In the callback, confirm if the device supports the cluster and retrieve the endpoint.

3. **Bind the Device**:
   - Use `esp_zb_zdo_device_bind_req` to bind the device to the coordinator.

4. **Process Reports**:
   - Implement a handler for the IAS Zone Cluster to process state change reports from the window sensor.

This process ensures that your application can discover, bind, and use the window sensor effectively. Let me know if you need further clarification!

If the device supports the required cluster (e.g., **IAS Zone Cluster**, `0x0500`), the response to the **Match Descriptor Request** will indicate success, and the callback function will provide the following key information:

1. **ZDO Status**:
   - The `zdo_status` parameter in the callback will be `ESP_ZB_ZDP_STATUS_SUCCESS`, indicating that the device supports the requested cluster.

2. **Device Address**:
   - The `addr` parameter will contain the **short address** of the device that matched the cluster.

3. **Endpoint**:
   - The `endpoint` parameter will specify the **endpoint** on the device where the cluster is implemented. Zigbee devices can have multiple endpoints, each supporting different clusters.

---

### **Example Response in the Callback**
Here’s what the callback might look like if the device supports the IAS Zone Cluster:

#### **Callback Code**
```c
static void match_desc_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Device 0x%04x supports the IAS Zone Cluster on endpoint %d", addr, endpoint);
        // Proceed to bind the device or configure it
        bind_device(addr, endpoint);
    } else {
        ESP_LOGW(TAG, "Device 0x%04x does not support the IAS Zone Cluster", addr);
    }
}
```

#### **Example Log Output**
If the device is a window sensor and supports the IAS Zone Cluster, the log might look like this:
```
I (12345) ZIGBEE: Device 0x1234 supports the IAS Zone Cluster on endpoint 1
```

---

### **What Happens Next**
If the response indicates that the device supports the IAS Zone Cluster:
1. **Bind the Device**:
   - Use the short address (`addr`) and endpoint (`endpoint`) to bind the device to the coordinator.

2. **Configure Reporting (Optional)**:
   - If the device supports reporting, configure it to send periodic updates or state changes (e.g., open/closed).

3. **Handle Incoming Reports**:
   - Implement a handler for the IAS Zone Cluster to process reports from the device.

---

### **If the Device Does Not Support the Cluster**
If the device does not support the IAS Zone Cluster:
- The `zdo_status` parameter will indicate failure (e.g., `ESP_ZB_ZDP_STATUS_FAIL`).
- The callback will log a warning:
  ```
  W (12345) ZIGBEE: Device 0x1234 does not support the IAS Zone Cluster
  ```

---

### **Summary**
If the device is a window sensor and supports the IAS Zone Cluster:
- The `zdo_status` in the callback will be `ESP_ZB_ZDP_STATUS_SUCCESS`.
- The `addr` will contain the device's short address.
- The `endpoint` will specify the endpoint where the IAS Zone Cluster is implemented.

You can then proceed to bind the device and handle its reports in your application.


To receive and handle state changes from the window sensor, you need to implement a handler for the **IAS Zone Cluster (`0x0500`)**. The sensor will send **Zone Status Change Notifications** to the coordinator whenever its state changes (e.g., open/closed). Here's how you can set this up:

---

### **Steps to Handle State Changes**

#### **1. Bind the Sensor to the Coordinator**
Ensure that the sensor is bound to the coordinator. This allows the sensor to send reports (e.g., state changes) to the coordinator.

#### **2. Enable Reporting for the IAS Zone Cluster**
The sensor must be configured to send **Zone Status Change Notifications**. Most IAS Zone devices automatically send these notifications when their state changes, but you can explicitly configure reporting if needed.

#### **3. Implement a Handler for the IAS Zone Cluster**
You need to implement a Zigbee cluster handler to process incoming notifications from the IAS Zone Cluster.

---

### **Zone Status Change Notification**
The **Zone Status Change Notification** is a command sent by the sensor to the coordinator. It contains the following key information:
- **Zone Status**: A bitmask indicating the current state of the sensor (e.g., open/closed, tampered, etc.).
- **Extended Status**: Additional status information (usually unused).
- **Zone ID**: The ID of the zone (e.g., the endpoint of the sensor).
- **Delay**: A delay value (optional).

---

### **Example Code to Handle Notifications**

#### **1. Register a Callback for the IAS Zone Cluster**
You need to register a callback to handle incoming commands for the IAS Zone Cluster.

```c
void register_ias_zone_handler(void) {
    esp_zb_zcl_add_cluster_handler(ZB_ZCL_CLUSTER_ID_IAS_ZONE, ias_zone_cluster_handler);
}
```

#### **2. Implement the Cluster Handler**
The cluster handler processes incoming commands for the IAS Zone Cluster. For a **Zone Status Change Notification**, you can extract the `zone_status` field to determine the sensor's state.

```c
static void ias_zone_cluster_handler(esp_zb_zcl_cmd_t *cmd_info) {
    if (cmd_info->cmd_id == ZB_ZCL_CMD_IAS_ZONE_STATUS_CHANGE_NOTIFICATION) {
        // Extract the Zone Status Change Notification payload
        uint16_t zone_status = cmd_info->payload[0] | (cmd_info->payload[1] << 8);
        uint8_t zone_id = cmd_info->payload[2];

        ESP_LOGI(TAG, "Zone Status Change Notification received");
        ESP_LOGI(TAG, "Zone Status: 0x%04x", zone_status);
        ESP_LOGI(TAG, "Zone ID: %d", zone_id);

        // Check the zone status bitmask
        if (zone_status & 0x0001) {
            ESP_LOGI(TAG, "Sensor state: Open");
        } else {
            ESP_LOGI(TAG, "Sensor state: Closed");
        }

        if (zone_status & 0x0004) {
            ESP_LOGW(TAG, "Tamper detected!");
        }
    } else {
        ESP_LOGW(TAG, "Unhandled IAS Zone command: 0x%02x", cmd_info->cmd_id);
    }
}
```

---

### **3. Process the Zone Status Bitmask**
The `zone_status` field is a bitmask that provides information about the sensor's state. Here are the common bits:

| **Bit** | **Hex Mask** | **Description**                  |
|---------|--------------|----------------------------------|
| 0       | `0x0001`     | Alarm 1 (e.g., open/closed).    |
| 1       | `0x0002`     | Alarm 2 (optional).             |
| 2       | `0x0004`     | Tamper detected.               |
| 3       | `0x0008`     | Battery low.                   |
| 4       | `0x0010`     | Supervision reports.           |
| 5       | `0x0020`     | Restore reports.               |
| 6       | `0x0040`     | Trouble detected.              |
| 7       | `0x0080`     | AC mains fault.                |

You can use bitwise operations to check the state of each bit.

---

### **4. Example Log Output**
When the sensor sends a **Zone Status Change Notification**, the log might look like this:
```
I (12345) ZIGBEE: Zone Status Change Notification received
I (12346) ZIGBEE: Zone Status: 0x0001
I (12347) ZIGBEE: Zone ID: 1
I (12348) ZIGBEE: Sensor state: Open
```

If the sensor is tampered with:
```
I (12345) ZIGBEE: Zone Status Change Notification received
I (12346) ZIGBEE: Zone Status: 0x0005
I (12347) ZIGBEE: Zone ID: 1
I (12348) ZIGBEE: Sensor state: Open
W (12349) ZIGBEE: Tamper detected!
```

---

### **5. Optional: Configure Reporting**
If the sensor does not automatically send notifications, you can configure it to report changes using the **Configure Reporting** command for the IAS Zone Cluster.

#### **Code Example: Configure Reporting**
```c
esp_zb_zcl_configure_reporting_req_t reporting_req = {
    .cluster_id = ZB_ZCL_CLUSTER_ID_IAS_ZONE,
    .attr_id = ZB_ZCL_ATTR_IAS_ZONE_STATUS_ID,
    .direction = ESP_ZB_ZCL_CONFIGURE_REPORTING_SEND_REPORT,
    .min_interval = 1,  // Minimum reporting interval (in seconds)
    .max_interval = 60, // Maximum reporting interval (in seconds)
    .reportable_change = 1, // Report on any change
};

esp_zb_zcl_configure_reporting(&reporting_req, configure_reporting_cb, NULL);
```

#### **Callback for Configure Reporting**
```c
static void configure_reporting_cb(esp_zb_zcl_status_t status, void *user_ctx) {
    if (status == ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Reporting successfully configured");
    } else {
        ESP_LOGW(TAG, "Failed to configure reporting");
    }
}
```

---

### **Summary**
1. **Bind the Sensor**: Ensure the sensor is bound to the coordinator.
2. **Handle Notifications**: Implement a handler for the IAS Zone Cluster to process **Zone Status Change Notifications**.
3. **Process the Zone Status**: Use the `zone_status` bitmask to determine the sensor's state (e.g., open/closed, tampered).
4. **Optional Reporting**: Configure reporting if the sensor does not automatically send notifications.

This setup allows your application to receive and handle state changes (e.g., open/closed) from the window sensor. Let me know if you need further clarification!