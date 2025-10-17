// app_logic.c

#include "app_logic.h"
#include "esp_log.h"
#include "feature_motor.h" 

// --- 引用依赖组件的头文件 ---
#include "event_manager.h"
#include "feature_anim_player.h" 

// ******* 1. 新增：包含4G模块的头文件 *******
#include "feature_4g_ml307.h"

#define TAG "APP_LOGIC"

static void main_event_handler_task(void *pvParameters)
{
    QueueHandle_t queue = get_event_queue();
    AppEvent_t received_event;
    anim_type_t current_anim = ANIM_TYPE_AINI; 

    ESP_LOGI(TAG, "核心逻辑任务已启动，等待事件...");

    while(1) {
        if (xQueueReceive(queue, &received_event, portMAX_DELAY) == pdPASS) {
            
            switch (received_event.event_type) {
                
                case EVENT_BUTTON_SHORT_PRESS:
                    ESP_LOGI(TAG, "收到按键短按，切换动画...");
                    
                    current_anim++;
                    if (current_anim > ANIM_TYPE_ZUOGUOYOUPAN) { 
                        current_anim = ANIM_TYPE_AINI;
                    }
                    anim_player_switch_animation(current_anim);
                    break;

                case EVENT_BUTTON_LONG_PRESS:
                    ESP_LOGI(TAG, "收到长按事件，转发请求给4G模块执行HTTP测试...");
                    
                    Feature4GEvent_t event_to_4g_http;
                    event_to_4g_http.event_type = EVENT_4G_PERFORM_HTTP_TEST;
                    
                    // ***** 修改日志 *****
                    BaseType_t send_result_http = feature_4g_send_event(&event_to_4g_http);
                    if (send_result_http != pdPASS) {
                        ESP_LOGW(TAG, "发送HTTP测试事件到4G模块失败! 返回值: %d", send_result_http);
                    }
                    break;

                case EVENT_BUTTON_DOUBLE_CLICK:
                    ESP_LOGI(TAG, "收到双击事件，转发请求给4G模块执行MQTT测试...");

                    Feature4GEvent_t event_to_4g_mqtt;
                    event_to_4g_mqtt.event_type = EVENT_4G_PERFORM_MQTT_TEST;

                    // ***** 修改日志 *****
                    BaseType_t send_result_mqtt = feature_4g_send_event(&event_to_4g_mqtt);
                    if (send_result_mqtt != pdPASS) {
                        ESP_LOGW(TAG, "发送MQTT测试事件到4G模块失败! 返回值: %d", send_result_mqtt);
                    }
                    break;
                
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
        "main_event_task",
        4096,
        NULL,
        10, // 主逻辑任务优先级可以高一些，以保证快速响应
        NULL
    );
}