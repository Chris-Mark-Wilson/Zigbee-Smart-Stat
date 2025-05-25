idf_component_register(
    SRCS 
    "main.c"

    "LVGL_Driver/LVGL_Driver.c"
    "LVGL_UI/assets/presence_active.c"
    "LVGL_UI/assets/presence_inactive.c"
    "LVGL_UI/assets/window_closed.c"
    "LVGL_UI/assets/window_open.c"
    "LVGL_UI/ui_screens.c"
    "Zigbee/zigbee.c"
    "Sensors/sensors.c"
     "Buttons/button.c"
     "Helpers/helpers.c"
     "NVS/nvs_functions.c"
     "esp_touch/touch_bsp.c"
    "i2c_bsp/i2c_bsp.c"
    "Settings/settings.c"
    "LVGL_Driver/touch_driver.c"

    INCLUDE_DIRS 
    "./i2c_bsp"
    "./esp_touch"
    "./LVGL_Driver" 
    "./LVGL_UI" 
    "./Zigbee"
    "./Sensors"
    "./Buttons"
    "./Helpers"
    "./NVS"
    "./Settings"
   
    "." 
  
   
  
  
)


