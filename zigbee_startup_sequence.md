# Zigbee Smart Thermostat Startup Sequence

## 1. Initial Stack Setup
- Stack initialization begins with `ESP_ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY` (0x17)
- Display shows "Starting Zigbee stack..."
- Configuration is loaded from NVS storage

## 2. Network Formation
1. `ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP` (0x06) triggers
2. `esp_zb_bdb_start_top_level_commissioning()` called with `ESP_ZB_BDB_MODE_NETWORK_FORMATION`
3. Network formation process:
   - `ESP_ZB_BDB_SIGNAL_FORMATION` indicates formation status
   - If successful, moves to network steering
   - If failed, displays error and stops

## 3. Network Steering
- `ESP_ZB_BDB_SIGNAL_STEERING` indicates steering completion
- `nvs_check_for_paired_devices()` checks for existing devices
- If no paired devices:
  - Opens network with `open_network(180)` (3 minutes)
  - Displays "Put devices in pair mode..."

## 4. Device Join Process
When a device attempts to join:

1. **Association Phase**
   - `ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED` (0x12)
   - Device gets initial network access

2. **Announcement Phase**
   - `ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE`
   - Device announces itself
   - `identify_device_type()` determines if TRV or Window Sensor
   - Device info saved to NVS with `save_device_to_nvs()`

3. **Authorization Phase**
   - `ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED`
   - Confirms device is allowed on network
   - Triggers endpoint discovery

## 5. Device Configuration
1. **Endpoint Discovery**
   - `query_device_endpoints()` requests device endpoints
   - `active_ep_cb()` processes endpoint information

2. **Simple Descriptor**
   - `simple_desc_cb()` gets detailed endpoint info
   - Stores endpoint configuration

3. **Binding Process**
   - Initial 30 second delay (`AUTH_BIND_DELAY_MS`)
   - `binding_table_cb()` checks existing bindings
   - `bind_window_sensor()` attempts cluster binding
   - Retries on failure (max 3 attempts)

## 6. Status Codes
Important binding status codes:
- `0x3c` (60): Device Unavailable
- `0x84` (132): Not Active
- `0x85` (133): Table Full

## 7. Key Functions
```c
bind_window_sensor()        // Initiates binding process
binary_input_bind_cb()      // Handles binding response
configure_window_sensor_reporting() // Sets up sensor reporting
query_binding_table()       // Checks existing bindings
delayed_bind_attempt()      // Retries binding after delay
```

## 8. Error Handling
- Device unavailable triggers 30-second wait
- Binding failures trigger up to 3 retries
- Network closes automatically after successful pairing
- Maximum device limits enforced per device type

## 9. UI Integration
- Status updates sent via `ui_event_queue`
- Screen transitions handled by UI task
- Network status displayed on boot screen

## Next Steps
After successful initialization and device pairing:
1. Network closes automatically
2. UI transitions to main control screen
3. Normal operation begins with bound devices