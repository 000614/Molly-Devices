// app_logic.c

#include "app_logic.h"
#include "esp_log.h"
#include "feature_motor.h" 
#include <string.h> // ****** 1. 新增：为了使用strcpy，需要包含此头文件 ******

// --- 引用依赖组件的头文件 ---
#include "event_manager.h"
#include "feature_anim_player.h" 

// --- 引用4G模块的头文件 ---
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

                // ****** 2. 修改：将长按事件的行为从HTTP测试改为WebSocket连接 ******
                case EVENT_BUTTON_LONG_PRESS:
                    ESP_LOGI(TAG, "收到长按事件，请求连接WebSocket...");
                    
                    Feature4GEvent_t event_ws_connect;
                    event_ws_connect.event_type = EVENT_4G_WEBSOCKET_CONNECT;
                    // 设置要连接的服务器URL
                    strcpy(event_ws_connect.data.ws_connect.url, "wss://echo.websocket.events");
                    
                    BaseType_t send_result_connect = feature_4g_send_event(&event_ws_connect);
                    if (send_result_connect != pdPASS) {
                        ESP_LOGW(TAG, "发送WebSocket连接事件到4G模块失败! 返回值: %d", send_result_connect);
                    }
                    break;

                // // ****** 3. 修改：将双击事件的行为从MQTT测试改为发送WebSocket消息 ******
                // case EVENT_BUTTON_DOUBLE_CLICK:
                //     ESP_LOGI(TAG, "收到双击事件，请求发送WebSocket文本消息...");

                //     Feature4GEvent_t event_ws_send_text;
                //     event_ws_send_text.event_type = EVENT_4G_WEBSOCKET_SEND_TEXT;
                //     // 设置要发送的文本内容
                //     strcpy(event_ws_send_text.data.ws_send_text.text, "Hello, this is a test message from ESP32!");

                //     BaseType_t send_result_send = feature_4g_send_event(&event_ws_send_text);
                //     if (send_result_send != pdPASS) {
                //         ESP_LOGW(TAG, "发送WebSocket文本事件到4G模块失败! 返回值: %d", send_result_send);
                //     }
                //     break;
                
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
        10,
        NULL
    );
}