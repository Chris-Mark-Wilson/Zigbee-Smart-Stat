typedef struct esp_zb_zdo_signal_device_annce_params_s {
    uint16_t device_short_addr;           /*!< address of device that recently joined to network */
    esp_zb_ieee_addr_t   ieee_addr;       /*!< The 64-bit (IEEE) address assigned to the device. */
    uint8_t       capability;             /*!< The capability of the device. */
} esp_zb_zdo_signal_device_annce_params_t; //in zdo/esp_zigbee_zdo_command.h

typedef struct esp_zb_zdo_match_desc_req_param_s {
    uint16_t dst_nwk_addr;              /*!< NWK address that request sent to */
    uint16_t addr_of_interest;          /*!< NWK address of interest */
    uint16_t profile_id;                /*!< Profile ID to be match at the destination which refers to esp_zb_af_profile_id_t */
    uint8_t num_in_clusters;            /*!< The number of input clusters provided for matching cluster server */
    uint8_t num_out_clusters;           /*!< The number of output clusters provided for matching cluster client */
    uint16_t *cluster_list;             /*!< The pointer MUST point the uint16_t object with a size equal to num_in_clusters + num_out_clusters,
                                         * which will be used to match device. */
} esp_zb_zdo_match_desc_req_param_t; //in zdo/esp_zigbee_zdo_command.h

typedef enum {
    ESP_ZB_AF_HA_PROFILE_ID     = 0x0104U,  /** HA profile ID */
    ESP_ZB_AF_SE_PROFILE_ID     = 0x0109U,  /** SE profile ID */
    ESP_ZB_AF_ZLL_PROFILE_ID    = 0xC05EU,  /** ZLL profile ID */
    ESP_ZB_AF_GP_PROFILE_ID     = 0xA1E0U,  /** GreenPower profile ID */
} esp_zb_af_profile_id_t; // in zcl/esp_zigbee_zcl_common.h

typedef void (*esp_zb_zdo_match_desc_callback_t)(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx);
