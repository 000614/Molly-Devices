#ifndef APP_LOGIC_H
#define APP_LOGIC_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief 初始化核心逻辑模块
 * 
 * 这个函数会创建事件队列并启动事件处理任务。
 * 必须在其他依赖事件队列的模块启动前调用。
 */
void app_logic_init_start(void);

/**
 * @brief 获取应用程序的事件队列句柄
 * 
 * 其他模块（如按键、WiFi）可以通过此函数获取队列句柄，以便向核心逻辑任务发送事件。
 * @note 必须在 app_logic_init_start() 调用之后才能调用此函数。
 * 
 * @return QueueHandle_t 事件队列的句柄
 */
QueueHandle_t app_logic_get_event_queue(void);


#endif // APP_LOGIC_H