#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include "LVGL_Driver.h"
#include "lvgl.h"

typedef enum {
    SCREEN_BOOT,
    SCREEN_MAIN,
    SCREEN_SETTINGS,
    SCREEN_COUNT
} screen_id_t;

typedef struct {
    lv_obj_t *screen;
    union {
        // Boot screen widgets
        struct {
            lv_obj_t *status_label;
        } boot;
        
        // Main screen widgets
        struct {
            lv_obj_t *temp_label;     // Whole number part
            lv_obj_t *temp_decimal;   // Decimal part
            lv_obj_t *humid_label;
            lv_obj_t *presence_img;
            lv_obj_t *window_img;
            lv_obj_t *range_label;    // New label for range display
        } main;
        
        // Settings screen widgets
        struct {
            lv_obj_t *high_temp_slider;
            lv_obj_t *low_temp_slider;
            lv_obj_t *presence_range_slider;
            lv_obj_t *high_temp_label;
            lv_obj_t *low_temp_label;
            lv_obj_t *range_label;
        } settings;
    };
} screen_t;

extern screen_t g_screens[SCREEN_COUNT];
extern screen_id_t current_screen;
extern float g_current_range;     // Current detected range in meters 
void ui_init_screens(void);
void ui_switch_screen(screen_id_t screen);
void ui_update_boot_status(const char *status);
void ui_update_main_screen(float temp, float humidity, bool presence, bool window_open);
void ui_update_settings(uint8_t high_temp, uint8_t low_temp, uint8_t presence_range);

#endif