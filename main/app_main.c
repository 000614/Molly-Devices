#include "esp_log.h"

// 包含所有需要启动的组件
#include "bsp_button.h"


#include "event_manager.h"
#include "feature_anim_player.h"
#include "feature_input_handler.h"
#include "app_logic.h"
#include "storage_manager.h"
#define TAG "MAIN_APP"

void app_main(void)
{
    // 1. 初始化硬件 (BSP层)
    ESP_ERROR_CHECK(storage_init()); 
    // anim_player_init() 内部已经调用了 bsp_lcd_init()
    ESP_ERROR_CHECK(bsp_button_init()); 

    // 2. 初始化核心框架
    event_queue_init();

    // 3. 初始化并启动所有功能/服务任务
    ESP_ERROR_CHECK(anim_player_init());


    
    anim_player_task_start();
    input_handler_task_start();
    
    // 4. 最后启动核心逻辑任务，让它开始调度
    app_logic_task_start();

    ESP_LOGI(TAG, "所有任务已启动，系统运行中...");
}