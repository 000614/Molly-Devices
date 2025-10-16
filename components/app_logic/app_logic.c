#include "app_logic.h"
#include "esp_log.h"

// 【关键改动】包含新的事件定义文件
#include "app_events.h" 
#include "feature_anim_player.h"

#define TAG "APP_LOGIC"

// 【关键改动】将队列句柄定义为模块内部的静态变量
// 它不再是全局的 g_...，而是私有的 s_...
static QueueHandle_t s_app_event_queue = NULL;

// 任务处理函数本身基本不变，只是使用的队列变量名变了
static void main_event_handler_task(void *pvParameters)
{
    AppEvent_t received_event;
    anim_type_t current_anim = ANIM_TYPE_AINI; 

    ESP_LOGI(TAG, "核心逻辑任务已启动，等待事件...");

    while(1) {
        // 【关键改动】使用模块内部的静态队列句柄
        if (xQueueReceive(s_app_event_queue, &received_event, portMAX_DELAY) == pdPASS) {
            
            switch (received_event.event_type) {
                case EVENT_BUTTON_SHORT_PRESS:
                    ESP_LOGI(TAG, "收到按键事件，准备切换动画...");
                    current_anim++;
                    if (current_anim > ANIM_TYPE_ZUOGUOYOUPAN) { 
                        current_anim = ANIM_TYPE_AINI;
                    }
                    anim_player_switch_animation(current_anim);
                    break;
                
                default:
                    ESP_LOGW(TAG, "收到未知的事件类型: %d", received_event.event_type);
                    break;
            }
        }
    }
}

// 【关键改动】实现新的初始化函数
void app_logic_init_start(void)
{
    // 1. 创建队列 (原 event_queue_init 的内容)
    s_app_event_queue = xQueueCreate(10, sizeof(AppEvent_t));
    if (s_app_event_queue == NULL) {
        ESP_LOGE(TAG, "核心逻辑事件队列创建失败!");
        // 在实际项目中，这里可能需要一个panic或重启
        return;
    } else {
        ESP_LOGI(TAG, "核心逻辑事件队列创建成功!");
    }

    // 2. 创建事件处理任务 (原 app_logic_task_start 的内容)
    xTaskCreate(
        main_event_handler_task,
        "main_event_task",
        4096,             
        NULL,             
        10,               
        NULL              
    );
}

// 【关键改动】实现获取队列句柄的函数
QueueHandle_t app_logic_get_event_queue(void)
{
    // 确保队列已经被创建
    configASSERT(s_app_event_queue != NULL);
    return s_app_event_queue;
}