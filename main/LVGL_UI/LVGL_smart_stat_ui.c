#include "LVGL_smart_stat_ui.h"
// #include <cstddef>

/**********************
 *      TYPEDEFS
 **********************/
typedef enum {
    DISP_SMALL,
    DISP_MEDIUM,
    DISP_LARGE,
} disp_size_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
// static void Onboard_create(lv_obj_t * parent);

// static void ta_event_cb(lv_event_t * e);
void example1_increase_lvgl_tick(lv_timer_t * t);
/**********************
 *  STATIC VARIABLES
 **********************/
static disp_size_t disp_size;

static lv_obj_t * tv;
lv_style_t style_text_muted;
lv_style_t style_title;
static lv_style_t style_icon;
static lv_style_t style_bullet;

// Declare global labels for temperature, presence, and TRV
 lv_obj_t *label_temp;
 lv_obj_t *label_presence;
 lv_obj_t *label_trv;



static const lv_font_t * font_large;
static const lv_font_t * font_normal;

static lv_timer_t * auto_step_timer;

static lv_timer_t * meter2_timer;

lv_obj_t * SD_Size;
lv_obj_t * FlashSize;
lv_obj_t * Board_angle;
lv_obj_t * RTC_Time;
lv_obj_t * Wireless_Scan;



void IRAM_ATTR auto_switch(lv_timer_t * t)
{
  uint16_t page = lv_tabview_get_tab_act(tv);

  if (page == 0) { 
    lv_tabview_set_act(tv, 1, LV_ANIM_ON); 
  } else if (page == 3) {
    lv_tabview_set_act(tv, 2, LV_ANIM_ON); 
  }
}
void Lvgl_smart_stat_ui(void){

  disp_size = DISP_SMALL;                            

  // font_large = LV_FONT_DEFAULT;                             
  // font_normal = LV_FONT_DEFAULT;                         
  
  lv_coord_t tab_h;
  tab_h = 45;
  #if LV_FONT_MONTSERRAT_38
    font_large     = &lv_font_montserrat_38;
  #else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_18 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
  #endif
  #if LV_FONT_MONTSERRAT_12 
    font_normal    = &lv_font_montserrat_12;
  #else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_12 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
  #endif
  
  lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, font_large);
    // lv_obj_del(lv_scr_act());  // Delete the old screen if it exists
 
    // lv_obj_t * new_screen = lv_obj_create(NULL);  // Create a blank screen
    // lv_scr_load(new_screen);                     // Replace current screen with the new one

    // Just use the screen directly — no tabview
    lv_obj_t * screen = lv_scr_act();
    lv_obj_set_size(screen, LV_PCT(100), LV_PCT(100));  // Full screen
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFFFF), 0);  // Set background color to white
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);  // Set opacity to cover
    lv_obj_set_style_pad_all(screen, 0, 0);  // Set padding to 0
    lv_obj_set_style_pad_row(screen, 0, 0);  // Set row padding to 0
    lv_obj_set_style_pad_column(screen, 0, 0);  // Set column padding to 0
    lv_obj_set_style_border_width(screen, 0, 0);  // Set border width to 0
    lv_obj_set_style_radius(screen, 0, 0);  // Set border radius to 0
    lv_obj_set_style_outline_width(screen, 0, 0);  // Set outline width to 0


    // Create large temperature label
    label_temp = lv_label_create(screen);
    lv_obj_add_style(label_temp, &style_title, 0);
    lv_label_set_text(label_temp, "22.5°C");
    lv_obj_align(label_temp, LV_ALIGN_CENTER, 0, 0);  // Center it
    // lv_disp_set_rotation(NULL, LV_DISP_ROT_90);  // Rotate 90° clockwise

    // // Optional: add a button to cycle views (add humidity etc.)
    // lv_obj_t * btn = lv_btn_create(screen);
    // lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    // lv_obj_t * label_btn = lv_label_create(btn);
    // lv_label_set_text(label_btn, "Next");

    // lv_obj_add_event_cb(btn, button_event_handler, LV_EVENT_CLICKED, NULL);
}
  
  


void Lvgl_smart_stat_ui_close(void)
{
  /*Delete all animation*/
  lv_anim_del(NULL, NULL);

     /* Only delete the timer if it was created */
     if (meter2_timer != NULL) {
      lv_timer_del(meter2_timer);
      meter2_timer = NULL;
  }

  lv_obj_clean(lv_scr_act());

  lv_style_reset(&style_text_muted);
  lv_style_reset(&style_title);
  lv_style_reset(&style_icon);
  lv_style_reset(&style_bullet);
}


/**********************
*   STATIC FUNCTIONS
**********************/

// static void Onboard_create(lv_obj_t * parent)
// {
//     // Create a panel to hold the entire UI
//     lv_obj_t * panel = lv_obj_create(parent);
//     lv_obj_set_size(panel, lv_pct(100), lv_pct(100)); // full screen
//     lv_obj_center(panel);
//     lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
//     lv_obj_set_style_pad_all(panel, 10, 0);
//     lv_obj_set_style_pad_row(panel, 15, 0);

//     // Title
//     lv_obj_t * title = lv_label_create(panel);
//     lv_label_set_text(title, "Smart Thermostat");
//     lv_obj_add_style(title, &style_title, 0);
//     lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

//     // Temperature label (large)
//     label_temp = lv_label_create(panel);
//     lv_label_set_text(label_temp, "22.5°C");
//     lv_obj_set_style_text_font(label_temp, font_large, 0);
//     lv_obj_set_style_text_align(label_temp, LV_TEXT_ALIGN_CENTER, 0);

//     // Humidity label (smaller)
//     label_humidity = lv_label_create(panel);
//     lv_label_set_text(label_humidity, "Humidity: 45%");
//     lv_obj_add_style(label_humidity, &style_text_muted, 0);

//     // Presence label (large)
//     label_presence = lv_label_create(panel);
//     lv_label_set_text(label_presence, "Presence: Yes");
//     lv_obj_set_style_text_font(label_presence, font_large, 0);

//     // TRV label (large)
//     label_trv = lv_label_create(panel);
//     lv_label_set_text(label_trv, "TRV: Heating");
//     lv_obj_set_style_text_font(label_trv, font_large, 0);

//     // Timer for refreshing values if needed
//     auto_step_timer = lv_timer_create(example1_increase_lvgl_tick, 1000, NULL); // Optional
// }


void example1_increase_lvgl_tick(lv_timer_t * t)
{
  // char buf[100]={0}; 
  
  // snprintf(buf, sizeof(buf), "%ld MB\r\n", SDCard_Size);
  // lv_textarea_set_placeholder_text(SD_Size, buf);
  // snprintf(buf, sizeof(buf), "%ld MB\r\n", Flash_Size);
  // lv_textarea_set_placeholder_text(FlashSize, buf);
  // if(Scan_finish)
  //   snprintf(buf, sizeof(buf), "W: %d  B: %d    OK.\r\n",WIFI_NUM,BLE_NUM);
  //   // snprintf(buf, sizeof(buf), "WIFI: %d     ..OK.\r\n",WIFI_NUM);
  // else
  //   snprintf(buf, sizeof(buf), "W: %d  B: %d\r\n",WIFI_NUM,BLE_NUM);
  //   // snprintf(buf, sizeof(buf), "WIFI: %d  \r\n",WIFI_NUM);
  // lv_textarea_set_placeholder_text(Wireless_Scan, buf);
}

// static void ta_event_cb(lv_event_t * e)
// {
// }





