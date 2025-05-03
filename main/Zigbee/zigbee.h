#include <zigbee.c>



 void bdb_start_top_level_commissioning_cb(uint8_t mode_mask);


void user_find_cb(esp_zb_zdp_status_t zdo_status, uint16_t peer_addr, uint8_t peer_endpoint, void *user_ctx);

void find_temperature_sensor(esp_zb_zdo_match_desc_req_param_t *param, esp_zb_zdo_match_desc_callback_t user_cb, void *user_ctx);

void esp_app_buttons_handler(switch_func_pair_t *button_func_pair);

static esp_err_t deferred_driver_init(void);

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);

void esp_app_zb_attribute_handler(uint16_t cluster_id, const esp_zb_zcl_attribute_t *attribute);

static esp_err_t zb_attribute_reporting_handler(const esp_zb_zcl_report_attr_message_t *message);

static esp_err_t zb_read_attr_resp_handler(const esp_zb_zcl_cmd_read_attr_resp_message_t *message);

static esp_err_t zb_configure_report_resp_handler(const esp_zb_zcl_cmd_config_report_resp_message_t *message);

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message);

static esp_zb_cluster_list_t *custom_thermostat_clusters_create(esp_zb_thermostat_cfg_t *thermostat);

static esp_zb_ep_list_t *custom_thermostat_ep_create(uint8_t endpoint_id, esp_zb_thermostat_cfg_t *thermostat);