#include "LVGL_Driver.h"
#include "i2c_bsp.h"  
#include "touch_bsp.h"  
static const char *TAG_LVGL = "WS_LVGL";

static lv_color_t buf1[ LVGL_BUF_LEN ];
static lv_color_t buf2[ LVGL_BUF_LEN];
// static lv_color_t* buf1 = (lv_color_t*) heap_caps_malloc(LVGL_BUF_LEN , MALLOC_CAP_SPIRAM);
// static lv_color_t* buf2 = (lv_color_t*) heap_caps_malloc(LVGL_BUF_LEN , MALLOC_CAP_SPIRAM);
    

lv_disp_draw_buf_t disp_buf;                                                 // contains internal graphic buffer(s) called draw buffer(s)
lv_disp_drv_t disp_drv;                                                      // contains callback functions
static lv_indev_drv_t touch_drv;      // Touch input device driver
    
void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
  esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
#if (Direction == Normal) 
  int offsetx1 = area->x1 + 35;
  int offsetx2 = area->x2 + 35;
  int offsety1 = area->y1;
  int offsety2 = area->y2;
#elif (Direction == Rotate)
  int offsetx1 = area->x1;
  int offsetx2 = area->x2;
  int offsety1 = area->y1 + 35;
  int offsety2 = area->y2 + 35;
#endif
  // copy a buffer's content to a specific area of the display
  esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
void example_lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
        break;
    case LV_DISP_ROT_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
        break;
    case LV_DISP_ROT_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISP_ROT_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
lv_disp_t *disp;
void LVGL_Init(void)
{
    // Initialize LVGL library first
    ESP_LOGI(TAG_LVGL, "Initialize LVGL library");
    lv_init();

    // Initialize display buffer
    ESP_LOGI(TAG_LVGL, "Initialize display buffer");
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LVGL_BUF_LEN);

    // Initialize SPI and LCD
    spi_bus_config_t buscfg = 
    {
#if EXAMPLE_USE_SDCARD
    .miso_io_num = PIN_NUM_MISO,
#endif
    .mosi_io_num = PIN_NUM_MOSI,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz =  EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DMA_Line * sizeof(uint16_t), // RGB565 , 传输屏幕的1/10行的数据
  };
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  ESP_LOGI(TAG_LVGL, "Initialize SPI bus");
  // Initialize LCD panel
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_spi_config_t io_config = 
  {
    .dc_gpio_num = PIN_NUM_DC,
    .cs_gpio_num = PIN_NUM_CS,
    .pclk_hz = 20 * 1000 * 1000,
    .lcd_cmd_bits = 8,
    .lcd_param_bits = 8,
    .spi_mode = 0,
    .trans_queue_depth = 10,
    .on_color_trans_done = example_notify_lvgl_flush_ready,
    .user_ctx = &disp_drv,
  };
  sh8601_vendor_config_t vendor_config = 
  {
    .init_cmds = lcd_init_cmds,
    .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

  esp_lcd_panel_handle_t panel_handle = NULL;
  const esp_lcd_panel_dev_config_t panel_config = 
  {
    .reset_gpio_num = PIN_NUM_RST,
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = 16,
    .vendor_config = &vendor_config,
    .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  //ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    // Initialize I2C and touch ONCE, before registering touch driver
    ESP_LOGI(TAG_LVGL, "Initializing I2C and touch");
    I2C_master_Init();
    touch_Init();  // Initialize touch_bsp driver


    // Initialize LVGL display driver
    ESP_LOGI(TAG_LVGL, "Initialize LVGL display driver");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.physical_hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.physical_ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.drv_update_cb = example_lvgl_port_update_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    disp_drv.antialiasing = true;
    disp_drv.dpi = 180;  // Keep this value for 1.9" display

    // Register display driver
    ESP_LOGI(TAG_LVGL, "Register display driver to LVGL");
    disp = lv_disp_drv_register(&disp_drv);
    if (disp == NULL) {
        ESP_LOGE(TAG_LVGL, "Failed to register display driver");
        return;
    }

    // Initialize and register touch driver
    lv_indev_drv_init(&touch_drv);
    touch_drv.type = LV_INDEV_TYPE_POINTER;
    touch_drv.read_cb = touch_read_cb;
    
    lv_indev_t * touch_indev = lv_indev_drv_register(&touch_drv);
    if (touch_indev == NULL) {
        ESP_LOGE(TAG_LVGL, "Failed to register touch input driver");
        return;
    }

    // Create default style
    static lv_style_t style_default;
    lv_style_init(&style_default);
    lv_style_set_text_font(&style_default, &lv_font_montserrat_18);
    lv_style_set_text_letter_space(&style_default, 2);
    lv_style_set_text_line_space(&style_default, 2);
    
    // Apply style to default screen
    lv_obj_add_style(lv_scr_act(), &style_default, 0);

    // Initialize LVGL tick timer last
    ESP_LOGI(TAG_LVGL, "Install LVGL tick timer");
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));
}


void touch_read_cb(lv_indev_drv_t * drv, lv_indev_data_t * data) {
    uint16_t x, y;
    uint8_t touched = getTouch(&x, &y);
    
    if(touched) {
        data->state = LV_INDEV_STATE_PRESSED;
        #if (Direction == Normal)
            // Add 35 pixel offset to match display offset
            data->point.x = x - 0;  // Compensate for display offset if needed
            data->point.y = y;
        #else
            data->point.x = y;
            data->point.y = EXAMPLE_LCD_V_RES - x;
        #endif
        ESP_LOGI("TOUCH", "Raw x: %d, y: %d | Adjusted x: %d, y: %d", 
                 x, y, data->point.x, data->point.y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
