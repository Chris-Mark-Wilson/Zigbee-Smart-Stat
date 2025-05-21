#include "ui_screens.h"
#include "esp_log.h"
#include "Zigbee/zigbee.h"


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
    
    // Container for decimal and unit
    lv_obj_t *temp_suffix_container = lv_obj_create(temp_container);
    lv_obj_remove_style_all(temp_suffix_container);
    lv_obj_align(temp_suffix_container, LV_ALIGN_RIGHT_MID, 0, 8);
    
    // Decimal part (smaller)
    g_screens[SCREEN_MAIN].main.temp_decimal = lv_label_create(temp_suffix_container);
    lv_obj_set_style_text_font(g_screens[SCREEN_MAIN].main.temp_decimal, &lv_font_montserrat_24, 0);
    lv_obj_align(g_screens[SCREEN_MAIN].main.temp_decimal, LV_ALIGN_LEFT_MID, 0, 0);
    
    // Units (°C)
    lv_obj_t *temp_unit = lv_label_create(temp_suffix_container);
    lv_obj_set_style_text_font(temp_unit, &lv_font_montserrat_38, 0);
    lv_label_set_text(temp_unit, "°C");
    lv_obj_align(temp_unit, LV_ALIGN_LEFT_MID, 25, -5);
    
    // Humidity display (medium size)
    g_screens[SCREEN_MAIN].main.humid_label = lv_label_create(screen);
    lv_obj_set_style_text_font(g_screens[SCREEN_MAIN].main.humid_label, &lv_font_montserrat_38, 0);
    lv_obj_align(g_screens[SCREEN_MAIN].main.humid_label, LV_ALIGN_CENTER, 0, -30);
    
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

    // Window icon
    g_screens[SCREEN_MAIN].main.window_img = lv_img_create(status_container);
    lv_img_set_src(g_screens[SCREEN_MAIN].main.window_img, &window_closed);
    lv_obj_align(g_screens[SCREEN_MAIN].main.window_img, LV_ALIGN_RIGHT_MID, -60, 0);
}

static void create_settings_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    g_screens[SCREEN_SETTINGS].screen = screen;
    
    // High temperature slider (17-21°C)
    g_screens[SCREEN_SETTINGS].settings.high_temp_slider = lv_slider_create(screen);
    lv_slider_set_range(g_screens[SCREEN_SETTINGS].settings.high_temp_slider, 17, 21);
    
    // Low temperature slider (13-16°C)
    g_screens[SCREEN_SETTINGS].settings.low_temp_slider = lv_slider_create(screen);
    lv_slider_set_range(g_screens[SCREEN_SETTINGS].settings.low_temp_slider, 13, 16);
    
    // Presence range slider (3m-7m)
    g_screens[SCREEN_SETTINGS].settings.presence_range_slider = lv_slider_create(screen);
    lv_slider_set_range(g_screens[SCREEN_SETTINGS].settings.presence_range_slider, 3, 7);
    
    // Labels for values
    g_screens[SCREEN_SETTINGS].settings.high_temp_label = lv_label_create(screen);
    g_screens[SCREEN_SETTINGS].settings.low_temp_label = lv_label_create(screen);
    g_screens[SCREEN_SETTINGS].settings.range_label = lv_label_create(screen);
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

void ui_update_settings(uint8_t high_temp, uint8_t low_temp, uint8_t presence_range) {
    lv_slider_set_value(g_screens[SCREEN_SETTINGS].settings.high_temp_slider, high_temp, LV_ANIM_OFF);
    lv_slider_set_value(g_screens[SCREEN_SETTINGS].settings.low_temp_slider, low_temp, LV_ANIM_OFF);
    lv_slider_set_value(g_screens[SCREEN_SETTINGS].settings.presence_range_slider, presence_range, LV_ANIM_OFF);
    
    char high_temp_str[16];
    char low_temp_str[16];
    char range_str[16];
    
    snprintf(high_temp_str, sizeof(high_temp_str), "High: %d°C", high_temp);
    snprintf(low_temp_str, sizeof(low_temp_str), "Low: %d°C", low_temp);
    snprintf(range_str, sizeof(range_str), "Range: %dm", presence_range);
    
    lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.high_temp_label, high_temp_str);
    lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.low_temp_label, low_temp_str);
    lv_label_set_text(g_screens[SCREEN_SETTINGS].settings.range_label, range_str);
}
void ui_update_boot_status(const char *status,const char *header) {

    lv_label_set_text(g_screens[SCREEN_BOOT].boot.header_label, header);
    lv_label_set_text(g_screens[SCREEN_BOOT].boot.status_label, status);
}