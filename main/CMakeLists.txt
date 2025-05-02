idf_component_register(
    SRCS 
    "smartStatMain.c"
    "LCD_Driver/Vernon_ST7789T/Vernon_ST7789T.c" 
    "LCD_Driver/ST7789.c"
    "LVGL_Driver/LVGL_Driver.c"
    "LVGL_UI/LVGL_Example.c"
    INCLUDE_DIRS 
    "./LCD_Driver/Vernon_ST7789T" 
    "./LCD_Driver" 
    "./LVGL_Driver" 
    "./LVGL_UI" 
    "." 
  
)


