#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// 1. 定义所有可能的消息/事件类型
typedef enum {
    EVENT_NONE = 0,
    EVENT_BUTTON_SHORT_PRESS,   // 按钮短按事件
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

// 3. 声明一个全局的消息队列句柄
extern QueueHandle_t g_app_event_queue;

// 4. 初始化函数声明
void event_queue_init(void);

#endif // EVENT_MANAGER_H