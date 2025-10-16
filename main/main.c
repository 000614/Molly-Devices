#include "esp_log.h"

// 包含所有需要启动的组件
#include "bsp_button.h"
#include "event_manager.h"
#include "feature_anim_player.h"
#include "feature_button.h"
#include "feature_motor.h"
#include "app_logic.h"
#include "storage_manager.h"

#define TAG "MAIN_APP"

void app_main(void)
{
    ESP_ERROR_CHECK(storage_init()); 
    ESP_ERROR_CHECK(bsp_button_init()); 
    ESP_ERROR_CHECK(mada_initialize()); 
    ESP_ERROR_CHECK(anim_player_init());

    // 2. 初始化消息队列
    event_queue_init();

    

    // 消费者先启动
    app_logic_task_start();
    
    // 3. 初始化并启动所有功能/服务任务
    anim_player_task_start();
    button_scan_task_start();
    

    ESP_LOGI(TAG, "所有任务已启动，系统运行中...");
}