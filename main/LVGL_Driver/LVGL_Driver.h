#pragma once
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "esp_lcd_io_i2c.h"
#include "esp_touch/touch_bsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "esp_timer.h"
#include "esp_lcd_sh8601.h"
#include "./i2c_bsp/i2c_bsp.h"
#include "./esp_touch/touch_bsp.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"


#define LCD_HOST  SPI2_HOST

static SemaphoreHandle_t lvgl_mux = NULL;
#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (4 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     2

#define Rotate 1        //旋转90
#define Normal 0        //正常显示
#define Direction Normal

#if (Direction == Normal) 
  #define EXAMPLE_LCD_H_RES 170   //宽度 水平分辨率
  #define EXAMPLE_LCD_V_RES 320   //高度 竖直分辨率
#elif (Direction == Rotate)
  #define EXAMPLE_LCD_H_RES 320   //宽度 水平分辨率
  #define EXAMPLE_LCD_V_RES 170   //高度 竖直分辨率
#endif
#define LCD_DPI  180 // dots per inch, used for scaling in LVGL
#define EXAMPLE_LCD_DMA_Line (EXAMPLE_LCD_V_RES / 2)

#define EXAMPLE_USE_Disp       1
#define EXAMPLE_USE_TOUCH      1
#define EXAMPLE_USE_SDCARD     0

#if EXAMPLE_USE_SDCARD
#define PIN_NUM_MISO 19
#define PIN_NUM_SDCS 20
sdmmc_card_t *card = NULL; // Handle
#define SDlist "/sd_card"  // Directory, acts like a standard
esp_err_t s_example_write_file(const char *path, char *data);
esp_err_t s_example_read_file(const char *path,char *pxbuf,uint32_t *outLen);
float sd_card_get_value(void);
#endif

#define PIN_NUM_MOSI 4
#define PIN_NUM_CLK  5
#define PIN_NUM_CS   7
#define PIN_NUM_RST  14
#define PIN_NUM_DC   6
#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#if (EXAMPLE_USE_Disp == 1)
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = 
{
  #if (Direction == Normal) 
  //{0x36, (uint8_t []){0x00}, 1, 0},	
  #elif (Direction == Rotate)
  {0x36, (uint8_t []){0x70}, 1, 0},	
  #endif

  //{0x3a, (uint8_t []){0x55}, 1, 0},
  {0xb2, (uint8_t []){0x0c,0x0c,0x00,0x33,0x33}, 5, 0},
  {0xb7, (uint8_t []){0x35}, 1, 0},
  {0xbb, (uint8_t []){0x13}, 1, 0},
  {0xc0, (uint8_t []){0x2c}, 1, 0},
  {0xc2, (uint8_t []){0x01}, 1, 0},
  {0xc3, (uint8_t []){0x0b}, 1, 0},
  {0xc4, (uint8_t []){0x20}, 1, 0},
  {0xc6, (uint8_t []){0x0f}, 1, 0},
  {0xd0, (uint8_t []){0xa4,0xa1}, 2, 0},
  {0xd6, (uint8_t []){0xa1}, 1, 0},
  {0xe0, (uint8_t []){0x00,0x03,0x07,0x08,0x07,0x15,0x2A,0x44,0x42,0x0A,0x17,0x18,0x25,0x27}, 14, 0},
  {0xe1, (uint8_t []){0x00,0x03,0x08,0x07,0x07,0x23,0x2A,0x43,0x42,0x09,0x18,0x17,0x25,0x27}, 14, 0},
  {0x21, (uint8_t []){0x21}, 0, 0},
  {0x11, (uint8_t []){0x11}, 0, 120},
  {0x29, (uint8_t []){0x29}, 0, 0},
};
#endif


#define LVGL_BUF_LEN  (EXAMPLE_LCD_H_RES * 20)
#define EXAMPLE_LVGL_TICK_PERIOD_MS    2

extern lv_disp_draw_buf_t disp_buf;                                                 // contains internal graphic buffer(s) called draw buffer(s)
extern lv_disp_drv_t disp_drv;                                                      // contains callback functions
extern lv_disp_t *disp;    

bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
void example_lvgl_port_update_callback(lv_disp_drv_t *drv);
void example_increase_lvgl_tick(void *arg);
#ifndef LVGL_DRIVER_H
#define LVGL_DRIVER_H


void touch_read_cb(lv_indev_drv_t * drv, lv_indev_data_t * data);
bool touch_get_coordinates(uint16_t *x, uint16_t *y, uint8_t *touched);

#endif
void LVGL_Init(void);                     // Call this function to initialize the screen (must be called in the main function) !!!!!