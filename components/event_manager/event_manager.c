#include "event_manager.h"
#include "esp_log.h"

#define TAG "EVENT_MANAGER"

// 将全局变量改为静态变量，只能在本文件内访问
static QueueHandle_t s_app_event_queue = NULL; // <-- 修改点：添加 static 并重命名 (例如使用 s_ 前缀)

// 新增的实现函数，返回静态的队列句柄
QueueHandle_t get_event_queue(void) // <-- 新增的函数实现
{
    return s_app_event_queue;
}
// 初始化消息队列
void event_queue_init(void)
{
    // 创建一个队列，可以容纳10个 AppEvent_t 类型的消息
    // 对于大多数应用来说，10-20的深度是足够的
    s_app_event_queue = xQueueCreate(10, sizeof(AppEvent_t));
    if (s_app_event_queue == NULL) {
        ESP_LOGE(TAG, "消息队列创建失败!");
    } else {
        ESP_LOGI(TAG, "消息队列创建成功!");
    }
}