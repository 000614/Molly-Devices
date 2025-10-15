#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// 1. 定义所有可能的消息/事件类型
typedef enum {
    EVENT_NONE = 0,
    EVENT_BUTTON_SHORT_PRESS,   // 按钮短按事件
    // ... 其他事件
} EventType_t;

// 2. 定义消息的结构体
typedef struct {
    EventType_t event_type;
    void*       p_data;
    uint32_t    data_len;
} AppEvent_t;

#endif // APP_EVENTS_H