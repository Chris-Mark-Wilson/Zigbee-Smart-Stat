#include "settings.h"


void update_range_limit(float new_limit)
{
    if (new_limit < 2.0f) new_limit = 2.0f;
    if (new_limit > 7.0f) new_limit = 7.0f;
    
    g_range_limit = new_limit;
    ESP_LOGI(TAG, "Range limit updated to %.2fm", g_range_limit);
    
    // Force UI update when range limit changes
    ui_event_t event = {
        .target_screen = SCREEN_MAIN,
        .message = ""
    };
    xQueueSend(ui_event_queue, &event, 0);
}