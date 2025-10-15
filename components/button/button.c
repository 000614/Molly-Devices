
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/queue.h" // 明确包含 queue.h

#include "button.h"
// 引用依赖组件的头文件
#include "bsp_button.h"
#include "app_events.h"

#define TAG "BUTTON"

static void button_scan_task(void *pvParameters)
{
        // 将传入的 void* 参数强制转换回它本来的类型：QueueHandle_t
    QueueHandle_t event_queue = (QueueHandle_t)pvParameters;
    
    // 断言检查，确保传入的队列句柄是有效的
    configASSERT(event_queue != NULL);

    ESP_LOGI(TAG, "按键扫描任务已启动");
    bool last_button_state = false;

    while(1) {
        bool current_button_state = bsp_button_is_pressed();

        // 检测按钮按下的瞬间 (从“未按下”到“按下”的边沿)
        if (current_button_state && !last_button_state) {
            ESP_LOGI(TAG, "检测到按键按下，发送事件...");
            
            // 构造一个事件消息
            AppEvent_t event_msg = {
                .event_type = EVENT_BUTTON_SHORT_PRESS,
                .p_data = NULL,
                .data_len = 0
            };
            
            // 【关键改动】使用从参数中获取的局部队列句柄，而不是全局变量
            xQueueSend(event_queue, &event_msg, portMAX_DELAY);

            // 简单的软件消抖
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        last_button_state = current_button_state;

        // 任务轮询间隔
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// 启动函数现在需要接收一个队列句柄作为参数
void button_scan_task_start(QueueHandle_t queue_to_use)
{
    // 在创建任务时，将队列句柄作为参数传递进去
    xTaskCreate(
        button_scan_task,
        "btn_scan_task",
        8192,
        (void *)queue_to_use, // 将队列句柄作为参数传递
        5,
        NULL
    );
}