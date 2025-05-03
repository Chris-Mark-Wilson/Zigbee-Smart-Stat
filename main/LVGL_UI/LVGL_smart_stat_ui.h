// #pragma once

// #include "lvgl.h"
// #include "demos/lv_demos.h"
// // #include "SD_SPI.h"
// // #include "Wireless.h"

// #define EXAMPLE1_LVGL_TICK_PERIOD_MS  1000


// void Lvgl_Example1(void);
#ifndef LVGL_EXAMPLE_H
#define LVGL_EXAMPLE_H


#include "LVGL_Driver.h"
#include "lvgl.h"

// Declare labels as extern so other files (like app_main.c) can access them
extern lv_obj_t *label_temp;
extern lv_obj_t *label_presence;
extern lv_obj_t *label_trv;

// Function to initialize the LVGL UI
void Lvgl_smart_stat_ui(void);
// Function to clean up the LVGL UI
// This function should be called before creating a new UI to avoid memory leaks
void Lvgl_smart_stat_ui_close(void);

#endif // LVGL_EXAMPLE_H
