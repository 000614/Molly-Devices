#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// 1. 定义所有可能的消息/事件类型
typedef enum {
    EVENT_NONE = 0,
    EVENT_BUTTON_SHORT_PRESS,   // 按钮短按事件
    EVENT_BUTTON_LONG_PRESS,    // <-- 新增：按钮长按事件
    EVENT_BUTTON_DOUBLE_CLICK,  // <-- 新增：按钮双击事件
    // 未来可以在这里添加更多事件，例如：
    // EVENT_SENSOR_DATA_READY,
    // EVENT_WIFI_CONNECTED,
} EventType_t;

// 2. 定义消息的结构体
typedef struct {
    EventType_t event_type; // 必须有，用来区分消息类型
    void*       p_data;     // 可选，一个指针，用来传递额外的数据
    uint32_t    data_len;   // 可选，数据长度
} AppEvent_t;

// 5. 新增一个函数声明，用于获取消息队列句柄
QueueHandle_t get_event_queue(void); // <-- 新增这一行

// 4. 初始化函数声明
void event_queue_init(void);

#endif // EVENT_MANAGER_H