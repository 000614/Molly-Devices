#include "event_manager.h"
#include "esp_log.h"

#define TAG "EVENT_MANAGER"

// 定义全局消息队列句柄
QueueHandle_t g_app_event_queue = NULL;

// 初始化消息队列
void event_queue_init(void)
{
    // 创建一个队列，可以容纳10个 AppEvent_t 类型的消息
    // 对于大多数应用来说，10-20的深度是足够的
    g_app_event_queue = xQueueCreate(10, sizeof(AppEvent_t));
    if (g_app_event_queue == NULL) {
        ESP_LOGE(TAG, "消息队列创建失败!");
    } else {
        ESP_LOGI(TAG, "消息队列创建成功!");
    }
}