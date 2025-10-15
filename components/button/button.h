#ifndef BUTTON_H
#define BUTTON_H
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
/**
 * @brief 启动按钮扫描任务
 * 
 */
void button_scan_task_start(QueueHandle_t queue_to_use);

#endif // BUTTON_H