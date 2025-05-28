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

typedef struct esp_zb_zcl_config_report_record_s {
    esp_zb_zcl_report_direction_t direction; /*!< Direction field specifies whether values of the attribute are to be reported, or whether reports of the
                                                  attribute are to be received.*/
    uint16_t attributeID;                    /*!< Attribute ID to report */
    union {
        struct {
            uint8_t attrType;                /*!< Attribute type to report refer to zb_zcl_common.h zcl_attr_type */
            uint16_t min_interval;           /*!< Minimum reporting interval */
            uint16_t max_interval;           /*!< Maximum reporting interval */
            void *reportable_change;         /*!< Minimum change to attribute will result in report */
        };                                   /*!< Configurations to report sender. This is presented when the direction is ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
                                              *   when the receiver is configuring the sender to report the attributes.
                                              */
        struct {
            uint16_t timeout;                /*!< Timeout period */
        };                                   /*!< Configurations to report receiver. This is presented when the direction is ESP_ZB_ZCL_REPORT_DIRECTION_RECV,
                                              *   when the sender is configuring the receiver to receive to attributes report.
                                              */
    };
} esp_zb_zcl_config_report_record_t;

/**
 * @brief The Zigbee ZCL configure report command struct
 *
 */
typedef struct esp_zb_zcl_config_report_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint16_t clusterID;                                 /*!< Cluster ID to report */
    struct {
        uint8_t manuf_specific   : 2;                   /*!< Sent as manufacturer extension with code. */
        uint8_t direction        : 1;                   /*!< The command direction, refer to esp_zb_zcl_cmd_direction_t */
        uint8_t dis_defalut_resp : 1;                   /*!< Disable default response for this command. */
    };
    uint16_t manuf_code;                                /*!< The manufacturer code sent with the command. */
    uint16_t record_number;                             /*!< Number of report configuration record in the record_field */
    esp_zb_zcl_config_report_record_t *record_field;    /*!< Report configuration records, @ref esp_zb_zcl_config_report_record_s */
} esp_zb_zcl_config_report_cmd_t;