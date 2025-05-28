#include "ui_screens.h"
#include "esp_log.h"
#include "Zigbee/zigbee.h"
#include "settings.h"


LV_IMG_DECLARE(presence_active);
LV_IMG_DECLARE(presence_inactive);
LV_IMG_DECLARE(window_open);
LV_IMG_DECLARE(window_closed);

screen_t g_screens[SCREEN_COUNT];
screen_id_t current_screen = SCREEN_BOOT;
float g_current_range = 0.0f;     // Current detected range in meters




static void create_boot_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    g_screens[SCREEN_BOOT].screen = screen;
    
    // Create header container (20% of screen height)
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, LV_PCT(100), LV_PCT(20));
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    

        // Add header label and store reference
    g_screens[SCREEN_BOOT].boot.header_label = lv_label_create(header);
    lv_obj_set_style_text_font(g_screens[SCREEN_BOOT].boot.header_label, 
                            &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(g_screens[SCREEN_BOOT].boot.header_label, 
                            LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(g_screens[SCREEN_BOOT].boot.header_label, "Paired: 0");
    lv_obj_center(g_screens[SCREEN_BOOT].boot.header_label);
    lv_obj_set_style_text_color(g_screens[SCREEN_BOOT].boot.header_label, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_text_opa(g_screens[SCREEN_BOOT].boot.header_label, LV_OPA_COVER, 0);
    
    
    // Create container for status label (remaining 80% of screen)
    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(80));
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
       
  

    
    // Create status label inside content container
    g_screens[SCREEN_BOOT].boot.status_label = lv_label_create(content);
    lv_obj_set_align(g_screens[SCREEN_BOOT].boot.status_label, LV_ALIGN_CENTER);
    lv_obj_set_width(g_screens[SCREEN_BOOT].boot.status_label, LV_PCT(90));
    lv_obj_set_style_text_font(g_screens[SCREEN_BOOT].boot.status_label, 
        &lv_font_montserrat_24, 0);
    lv_label_set_text(g_screens[SCREEN_BOOT].boot.status_label, "Initializing...");
    lv_obj_set_style_text_color(g_screens[SCREEN_BOOT].boot.status_label, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_text_opa(g_screens[SCREEN_BOOT].boot.status_label, LV_OPA_COVER, 0);
}

static void create_main_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    g_screens[SCREEN_MAIN].screen = screen;
    
    // Create container for temperature display
    lv_obj_t *temp_container = lv_obj_create(screen);
    lv_obj_remove_style_all(temp_container);
    lv_obj_align(temp_container, LV_ALIGN_TOP_MID, 30, 10);
    lv_obj_set_size(temp_container, 200, 80);  // Set fixed size for container
    
    // Main temperature numbers (large)
    g_screens[SCREEN_MAIN].main.temp_label = lv_label_create(temp_container);
    lv_obj_set_style_text_font(g_screens[SCREEN_MAIN].main.temp_label, &lv_font_montserrat_48, 0);
    lv_obj_align(g_screens[SCREEN_MAIN].main.temp_label, LV_ALIGN_LEFT_MID, 15, 0);
       lv_obj_set_style_text_color(g_screens[SCREEN_MAIN].main.temp_label, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_text_opa(g_screens[SCREEN_MAIN].main.temp_label, LV_OPA_COVER, 0);   
   
    
    // Container for decimal and unit
    lv_obj_t *temp_suffix_container = lv_obj_create(temp_container);
    lv_obj_remove_style_all(temp_suffix_container);
    lv_obj_align(temp_suffix_container, LV_ALIGN_RIGHT_MID, 0, 8);
    
    // Decimal part (smaller)
    g_screens[SCREEN_MAIN].main.temp_decimal = lv_label_create(temp_suffix_container);
    lv_obj_set_style_text_font(g_screens[SCREEN_MAIN].main.temp_decimal, &lv_font_montserrat_24, 0);
    lv_obj_align(g_screens[SCREEN_MAIN].main.temp_decimal, LV_ALIGN_LEFT_MID, 0, 0);
       lv_obj_set_style_text_color(g_screens[SCREEN_MAIN].main.temp_decimal, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_text_opa(g_screens[SCREEN_MAIN].main.temp_decimal, LV_OPA_COVER, 0);
    
    // Units (°C)
    lv_obj_t *temp_unit = lv_label_create(temp_suffix_container);
    lv_obj_set_style_text_font(temp_unit, &lv_font_montserrat_38, 0);
    lv_label_set_text(temp_unit, "°C");
    lv_obj_align(temp_unit, LV_ALIGN_LEFT_MID, 25, -5);
       lv_obj_set_style_text_color(temp_unit, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_text_opa(temp_unit, LV_OPA_COVER, 0);
    
    // Humidity display (medium size)
    g_screens[SCREEN_MAIN].main.humid_label = lv_label_create(screen);
    lv_obj_set_style_text_font(g_screens[SCREEN_MAIN].main.humid_label, &lv_font_montserrat_38, 0);
    lv_obj_align(g_screens[SCREEN_MAIN].main.humid_label, LV_ALIGN_CENTER, 0, -30);
       lv_obj_set_style_text_color(g_screens[SCREEN_MAIN].main.humid_label, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_text_opa(g_screens[SCREEN_MAIN].main.humid_label, LV_OPA_COVER, 0);
    
    // Create container for status icons at bottom
    lv_obj_t *status_container = lv_obj_create(screen);
    lv_obj_remove_style_all(status_container);
    lv_obj_align(status_container, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_size(status_container, 120, 140);
    
    // Presence icon and its range label
    lv_obj_t *presence_container = lv_obj_create(status_container);
    lv_obj_remove_style_all(presence_container);
    lv_obj_align(presence_container, LV_ALIGN_LEFT_MID, 60, 20);
    lv_obj_set_size(presence_container, 70, 140);

    g_screens[SCREEN_MAIN].main.presence_img = lv_img_create(presence_container);
    lv_img_set_src(g_screens[SCREEN_MAIN].main.presence_img, &presence_inactive);
    lv_obj_align(g_screens[SCREEN_MAIN].main.presence_img, LV_ALIGN_TOP_MID, 0, 0);

    // Add range label under presence icon
    g_screens[SCREEN_MAIN].main.range_label = lv_label_create(presence_container);
    lv_obj_set_style_text_font(g_screens[SCREEN_MAIN].main.range_label, &lv_font_montserrat_18, 0);
    lv_label_set_text(g_screens[SCREEN_MAIN].main.range_label, "0cm");
    lv_obj_align(g_screens[SCREEN_MAIN].main.range_label, LV_ALIGN_BOTTOM_MID, 0, -30);
       lv_obj_set_style_text_color(g_screens[SCREEN_MAIN].main.range_label, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_text_opa(g_screens[SCREEN_MAIN].main.range_label, LV_OPA_COVER, 0);

    // Window icon
    g_screens[SCREEN_MAIN].main.window_img = lv_img_create(status_container);
    lv_img_set_src(g_screens[SCREEN_MAIN].main.window_img, &window_closed);
    lv_obj_align(g_screens[SCREEN_MAIN].main.window_img, LV_ALIGN_RIGHT_MID, -60, 0);
}

static void create_settings_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    g_screens[SCREEN_SETTINGS].screen = screen;
    char buf[32];
    
    // Create main container for settings
    lv_obj_t *settings_container = lv_obj_create(screen);
    lv_obj_remove_style_all(settings_container);
    lv_obj_set_size(settings_container, LV_PCT(90), LV_PCT(70));
    lv_obj_align(settings_container, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_flex_flow(settings_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(settings_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(settings_container, 20, 0);  // Space between rows

    // High temperature section
    lv_obj_t *high_temp_container = lv_obj_create(settings_container);
    lv_obj_remove_style_all(high_temp_container);
    lv_obj_set_size(high_temp_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(high_temp_container, LV_FLEX_FLOW_COLUMN);
    
    // High temperature label styling
    g_screens[SCREEN_SETTINGS].settings.high_temp_label = lv_label_create(high_temp_container);
    lv_obj_set_style_text_font(g_screens[SCREEN_SETTINGS].settings.high_temp_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(g_screens[SCREEN_SETTINGS].settings.high_temp_label, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_text_opa(g_screens[SCREEN_SETTINGS].settings.high_temp_label, LV_OPA_COVER, 0);
    snprintf(buf, sizeof(buf), "High: %d°C", g_target_high_temp);
    lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.high_temp_label, buf);
    
    g_screens[SCREEN_SETTINGS].settings.high_temp_slider = lv_slider_create(high_temp_container);
    lv_obj_set_width(g_screens[SCREEN_SETTINGS].settings.high_temp_slider, LV_PCT(100));
    lv_slider_set_range(g_screens[SCREEN_SETTINGS].settings.high_temp_slider, 17, 22);
    lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.high_temp_slider, 
                    settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Low temperature section
    lv_obj_t *low_temp_container = lv_obj_create(settings_container);
    lv_obj_remove_style_all(low_temp_container);
    lv_obj_set_size(low_temp_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(low_temp_container, LV_FLEX_FLOW_COLUMN);
    
    // Low temperature label styling
    g_screens[SCREEN_SETTINGS].settings.low_temp_label = lv_label_create(low_temp_container);
    lv_obj_set_style_text_font(g_screens[SCREEN_SETTINGS].settings.low_temp_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(g_screens[SCREEN_SETTINGS].settings.low_temp_label, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_text_opa(g_screens[SCREEN_SETTINGS].settings.low_temp_label, LV_OPA_COVER, 0);
    snprintf(buf, sizeof(buf), "Low: %d°C", g_target_low_temp);
    lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.low_temp_label, buf);
    
    g_screens[SCREEN_SETTINGS].settings.low_temp_slider = lv_slider_create(low_temp_container);
    lv_obj_set_width(g_screens[SCREEN_SETTINGS].settings.low_temp_slider, LV_PCT(100));
    lv_slider_set_range(g_screens[SCREEN_SETTINGS].settings.low_temp_slider, 13, 20);
        lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.low_temp_slider, 
                    settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Presence range section
    lv_obj_t *range_container = lv_obj_create(settings_container);
    lv_obj_remove_style_all(range_container);
    lv_obj_set_size(range_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(range_container, LV_FLEX_FLOW_COLUMN);
    
    // Presence range label styling
    g_screens[SCREEN_SETTINGS].settings.range_label = lv_label_create(range_container);
    lv_obj_set_style_text_font(g_screens[SCREEN_SETTINGS].settings.range_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(g_screens[SCREEN_SETTINGS].settings.range_label, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_text_opa(g_screens[SCREEN_SETTINGS].settings.range_label, LV_OPA_COVER, 0);
    snprintf(buf, sizeof(buf), "Range: %.1fm", g_range_limit);
    lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.range_label, buf);
    
    g_screens[SCREEN_SETTINGS].settings.presence_range_slider = lv_slider_create(range_container);
    lv_obj_set_width(g_screens[SCREEN_SETTINGS].settings.presence_range_slider, LV_PCT(100));
    lv_slider_set_range(g_screens[SCREEN_SETTINGS].settings.presence_range_slider, 0, 7);
        lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.presence_range_slider, 
                    settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
      // Room mask section
    lv_obj_t *room_container = lv_obj_create(settings_container);
    lv_obj_remove_style_all(room_container);
    lv_obj_set_size(room_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(room_container, LV_FLEX_FLOW_COLUMN);
    
    // Channel label styling
    g_screens[SCREEN_SETTINGS].settings.room_label = lv_label_create(room_container);
    lv_obj_set_style_text_font(g_screens[SCREEN_SETTINGS].settings.room_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(g_screens[SCREEN_SETTINGS].settings.room_label, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_text_opa(g_screens[SCREEN_SETTINGS].settings.room_label, LV_OPA_COVER, 0);
    snprintf(buf, sizeof(buf), "Room: %d", g_room);
    lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.room_label, buf);
    
    g_screens[SCREEN_SETTINGS].settings.room_slider = lv_slider_create(room_container);
    lv_obj_set_width(g_screens[SCREEN_SETTINGS].settings.room_slider, LV_PCT(100));
    lv_slider_set_range(g_screens[SCREEN_SETTINGS].settings.room_slider, 1, MAX_ROOMS);
        lv_obj_add_event_cb(g_screens[SCREEN_SETTINGS].settings.room_slider, 
                    settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Apply styles to all sliders
    lv_obj_t * sliders[] = {
        g_screens[SCREEN_SETTINGS].settings.high_temp_slider,
        g_screens[SCREEN_SETTINGS].settings.low_temp_slider,
        g_screens[SCREEN_SETTINGS].settings.presence_range_slider,
        g_screens[SCREEN_SETTINGS].settings.room_slider
    };
    
    for(int i = 0; i < 3; i++) {
        lv_obj_set_style_pad_top(sliders[i], 15, 0);    // Space above slider
        lv_obj_set_style_pad_bottom(sliders[i], 15, 0); // Space below slider
        lv_obj_add_flag(sliders[i], LV_OBJ_FLAG_ADV_HITTEST);  // Better touch handling
    }

      // Create button container at bottom
    lv_obj_t *button_container = lv_obj_create(screen);
    lv_obj_remove_style_all(button_container);
    lv_obj_set_size(button_container, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_align(button_container, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Create save button
    g_screens[SCREEN_SETTINGS].settings.save_btn = lv_btn_create(button_container);
    lv_obj_set_size(g_screens[SCREEN_SETTINGS].settings.save_btn, LV_PCT(45), 40);
    lv_obj_set_style_bg_color(g_screens[SCREEN_SETTINGS].settings.save_btn, lv_color_make(0, 150, 0), 0);
  
    
    // Create cancel button
    g_screens[SCREEN_SETTINGS].settings.cancel_btn = lv_btn_create(button_container);
    lv_obj_set_size(g_screens[SCREEN_SETTINGS].settings.cancel_btn, LV_PCT(45), 40);
    lv_obj_set_style_bg_color(g_screens[SCREEN_SETTINGS].settings.cancel_btn, lv_color_make(150, 0, 0), 0);
    
    // Button labels styling
    lv_obj_t *save_label = lv_label_create(g_screens[SCREEN_SETTINGS].settings.save_btn);
    lv_obj_set_style_text_font(save_label, &lv_font_montserrat_18, 0);
    lv_label_set_text(save_label, "Save");
    lv_obj_center(save_label);
    lv_obj_set_style_text_color(save_label, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_text_opa(save_label, LV_OPA_COVER, 0);

    lv_obj_t *cancel_label = lv_label_create(g_screens[SCREEN_SETTINGS].settings.cancel_btn);
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_18, 0);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
    lv_obj_set_style_text_color(cancel_label, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_text_opa(cancel_label, LV_OPA_COVER, 0);
}

void ui_init_screens(void) {
    create_boot_screen();
    create_main_screen();
    create_settings_screen();
    
    // Start with boot screen
    lv_scr_load(g_screens[SCREEN_BOOT].screen);
}

void ui_switch_screen(screen_id_t screen) {
    if (screen >= SCREEN_COUNT || screen == current_screen) return;
    
    lv_scr_load_anim(g_screens[screen].screen, 
                     LV_SCR_LOAD_ANIM_MOVE_LEFT, 
                     300, 0, false);
    current_screen = screen;
}

void ui_update_main_screen(float temp, float humidity, bool presence, bool is_window_open)
{
    if (!g_screens[SCREEN_MAIN].main.temp_label) return;
    
    static char temp_whole[16];
    static char temp_decimal[8];
    static char humid_buf[32];
    
    // Split temperature into whole and decimal parts
    int temp_int = (int)temp;
    int temp_dec = (int)((temp - temp_int) * 10);
    
    snprintf(temp_whole, sizeof(temp_whole), "%d", temp_int);
    snprintf(temp_decimal, sizeof(temp_decimal), ".%d", temp_dec);
    snprintf(humid_buf, sizeof(humid_buf), "%.0f%%", humidity);  // Removed space before %
    
    lv_label_set_text(g_screens[SCREEN_MAIN].main.temp_label, temp_whole);
    lv_label_set_text(g_screens[SCREEN_MAIN].main.temp_decimal, temp_decimal);
    lv_label_set_text(g_screens[SCREEN_MAIN].main.humid_label, humid_buf);
    
    // Update status icons
    lv_img_set_src(g_screens[SCREEN_MAIN].main.presence_img, 
                   presence ? &presence_active : &presence_inactive);
    lv_img_set_src(g_screens[SCREEN_MAIN].main.window_img,
                   is_window_open ? &window_open : &window_closed);

    // Update range display
    static char range_buf[16];
    float range_in_meters = g_current_range / 100.0f;  // Convert cm to meters
    snprintf(range_buf, sizeof(range_buf), "%.1fm", range_in_meters);
    lv_label_set_text(g_screens[SCREEN_MAIN].main.range_label, range_buf);
}

void ui_update_settings(uint8_t high_temp, uint8_t low_temp, float presence_range, uint8_t room) {



    lv_slider_set_value(g_screens[SCREEN_SETTINGS].settings.high_temp_slider, high_temp, LV_ANIM_OFF);
    lv_slider_set_value(g_screens[SCREEN_SETTINGS].settings.low_temp_slider, low_temp, LV_ANIM_OFF);
    lv_slider_set_value(g_screens[SCREEN_SETTINGS].settings.presence_range_slider, presence_range/100, LV_ANIM_OFF);
    lv_slider_set_value(g_screens[SCREEN_SETTINGS].settings.room_slider, room, LV_ANIM_OFF);
    
    char high_temp_str[16];
    char low_temp_str[16];
    char range_str[16];
    char room_str[16];
    //range limit is in centimeters, convert to meters for display
    snprintf(high_temp_str, sizeof(high_temp_str), "High: %d°C", high_temp);
    snprintf(low_temp_str, sizeof(low_temp_str), "Low: %d°C", low_temp);
    snprintf(range_str, sizeof(range_str), "Range: %.1fm", presence_range/100);
    snprintf(room_str,sizeof(room_str), "room: %d", room);
    
    lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.high_temp_label, high_temp_str);
    lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.low_temp_label, low_temp_str);
    lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.range_label, range_str);
    lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.room_label, room_str);
}
void ui_update_boot_status(const char *status,const char *header) {

    lv_label_set_text(g_screens[SCREEN_BOOT].boot.header_label, header);
    lv_label_set_text(g_screens[SCREEN_BOOT].boot.status_label, status);
}