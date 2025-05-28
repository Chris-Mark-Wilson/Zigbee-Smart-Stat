#ifndef UI_EVENTS_H
#define UI_EVENTS_H

#include "freertos/queue.h"
#include "ui_screens.h"

typedef struct {
    char message[100];
    screen_id_t target_screen;
    uint8_t window_sensor_count;
    uint8_t trv_count;
} ui_event_t;

extern QueueHandle_t ui_event_queue;


#endif // UI_EVENTS_H