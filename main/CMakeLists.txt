idf_component_register(
    SRCS 
    "smartStatMain.c"
    "LCD_Driver/Vernon_ST7789T/Vernon_ST7789T.c" 
    "LCD_Driver/ST7789.c"
    "LVGL_Driver/LVGL_Driver.c"
    "LVGL_UI/assets/presence_active.c"
    "LVGL_UI/assets/presence_inactive.c"
    "LVGL_UI/assets/window_closed.c"
    "LVGL_UI/assets/window_open.c"
    "LVGL_UI/ui_screens.c"
    "Zigbee/zigbee.c"
    "Sensors/sensors.c"
     "Buttons/button.c"
    INCLUDE_DIRS 
    "./LCD_Driver/Vernon_ST7789T" 
    "./LCD_Driver" 
    "./LVGL_Driver" 
    "./LVGL_UI" 
    "./Zigbee"
    "./Sensors"
    "./Buttons"
    "." 
  
   
  
  
)


