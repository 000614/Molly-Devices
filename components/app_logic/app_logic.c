#include "app_logic.h"
#include "esp_log.h"
#include "feature_motor.h" // 包含震动马达功能层头文件
// 引用依赖组件的头文件
#include "event_manager.h"
// #include "feature_anim_player.h" // 需要调用它的API来切换动画

#define TAG "APP_LOGIC"

static void main_event_handler_task(void *pvParameters)
{
    // 通过函数获取句柄再发送消息
    QueueHandle_t queue = get_event_queue();
    // if (queue != NULL) { // 最好增加一个检查
    //     xQueueSend(queue, &event, portMAX_DELAY);
    // }

    AppEvent_t received_event;
    // 用于追踪当前动画状态，以便循环切换
    // anim_type_t current_anim = ANIM_TYPE_AINI; 

    ESP_LOGI(TAG, "核心逻辑任务已启动，等待事件...");

    while(1) {
        // 从队列中等待并接收消息
        // portMAX_DELAY 表示如果队列是空的，任务会进入阻塞状态，不消耗CPU
        if (xQueueReceive(queue, &received_event, portMAX_DELAY) == pdPASS) {
            
            switch (received_event.event_type) {
                
                case EVENT_BUTTON_SHORT_PRESS:
                    // ESP_LOGI(TAG, "收到按键事件，准备切换动画...");
                    
                    // // 简单的循环切换动画逻辑
                    // current_anim++;
                    // // 假设 ANIM_TYPE_ZUOGUOYOUPAN 是最后一个动画, 请根据你的枚举调整
                    // if (current_anim > ANIM_TYPE_ZUOGUOYOUPAN) { 
                    //     current_anim = ANIM_TYPE_AINI; // 回到第一个
                    // }
                    
                    // // 调用动画播放器组件的API
                    // anim_player_switch_animation(current_anim);
                    mada_stop();
                    break;

                case EVENT_BUTTON_LONG_PRESS:
                    ESP_LOGI(TAG, "收到长按事件，关闭屏幕显示...");
                    // anim_player_set_brightness(0x00);
                    mada_continuous_vibrate();
                    break;
                
                case EVENT_BUTTON_DOUBLE_CLICK:
                    ESP_LOGI(TAG, "收到双击事件，开启屏幕显示...");
                    // anim_player_set_brightness(0xFF);
                    break;

                // 未来可以在这里添加对其他事件的处理...
                
                default:
                    ESP_LOGW(TAG, "收到未知的事件类型: %d", received_event.event_type);
                    break;
            }
        }
    }
}

void app_logic_task_start(void)
{
    xTaskCreate(
        main_event_handler_task,
        "main_event_task", // 任务名
        4096,              // 堆栈大小
        NULL,              // 任务参数
        10,                // 任务优先级
        NULL               // 任务句柄
    );
}