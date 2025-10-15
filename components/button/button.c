#include "button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// 引用依赖组件的头文件
#include "bsp_button.h"
#include "event_manager.h"

#define TAG "INPUT_HANDLER"

static void button_scan_task(void *pvParameters)
{
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
            
            // 将消息发送到全局队列
            xQueueSend(g_app_event_queue, &event_msg, portMAX_DELAY);

            // 简单的软件消抖
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        last_button_state = current_button_state;

        // 任务轮询间隔
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void button_scan_task_start(void)
{
    xTaskCreate(
        button_scan_task,
        "btn_scan_task",
        8192,
        NULL,
        5,
        NULL
    );
}