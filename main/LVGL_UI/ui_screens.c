#include "ui_screens.h"
#include "esp_log.h"

static const char *TAG = "UI";

screen_t g_screens[SCREEN_COUNT];
screen_id_t current_screen = SCREEN_BOOT;

static void create_boot_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    g_screens[SCREEN_BOOT].screen = screen;
    
    // Create full screen label
    g_screens[SCREEN_BOOT].boot.status_label = lv_label_create(screen);
    lv_obj_set_align(g_screens[SCREEN_BOOT].boot.status_label, LV_ALIGN_CENTER);
    lv_obj_set_width(g_screens[SCREEN_BOOT].boot.status_label, LV_PCT(90));
    lv_obj_set_style_text_font(g_screens[SCREEN_BOOT].boot.status_label, 
        &lv_font_montserrat_24, 0);
    lv_label_set_text(g_screens[SCREEN_BOOT].boot.status_label, "Initializing...");
}

static void create_main_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    g_screens[SCREEN_MAIN].screen = screen;
    
    // Temperature display (large numbers)
    g_screens[SCREEN_MAIN].main.temp_label = lv_label_create(screen);
    lv_obj_set_style_text_font(g_screens[SCREEN_MAIN].main.temp_label, &lv_font_montserrat_48, 0);
    lv_obj_align(g_screens[SCREEN_MAIN].main.temp_label, LV_ALIGN_TOP_MID, 0, 20);
    
    // Humidity display
    g_screens[SCREEN_MAIN].main.humid_label = lv_label_create(screen);
    lv_obj_align(g_screens[SCREEN_MAIN].main.humid_label, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    
    // Presence and window status icons
    g_screens[SCREEN_MAIN].main.presence_img = lv_img_create(screen);
    g_screens[SCREEN_MAIN].main.window_img = lv_img_create(screen);
    // TODO: Add image sources and positioning
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

void ui_update_main_screen(float temp, float humidity, bool presence, bool window_open) {
    char temp_str[16];
    char humid_str[16];
    
    snprintf(temp_str, sizeof(temp_str), "%.1f°C", temp);
    snprintf(humid_str, sizeof(humid_str), "%.1f%%", humidity);
    
    lv_label_set_text(g_screens[SCREEN_MAIN].main.temp_label, temp_str);
    lv_label_set_text(g_screens[SCREEN_MAIN].main.humid_label, humid_str);
    
    // Update presence and window status icons
    if (presence) {
        lv_img_set_src(g_screens[SCREEN_MAIN].main.presence_img, "presence_active.png");
    } else {
        lv_img_set_src(g_screens[SCREEN_MAIN].main.presence_img, "presence_inactive.png");
    }
    if (window_open) {
        lv_img_set_src(g_screens[SCREEN_MAIN].main.window_img, "window_open.png");
    } else {
        lv_img_set_src(g_screens[SCREEN_MAIN].main.window_img, "window_closed.png");
    }
    lv_obj_align(g_screens[SCREEN_MAIN].main.presence_img, LV_ALIGN_CENTER, -50, 0);
    lv_obj_align(g_screens[SCREEN_MAIN].main.window_img, LV_ALIGN_CENTER, 50, 0);
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
void ui_update_boot_status(const char *status) {
    lv_label_set_text(g_screens[SCREEN_BOOT].boot.status_label, status);
}