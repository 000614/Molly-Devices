#include "feature_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// 引用依赖组件的头文件
#include "bsp_button.h"
#include "event_manager.h"

#define TAG "INPUT_HANDLER"

// --- 新增的宏定义 ---
#define DEBOUNCE_TIME_MS        50   // 按键消抖时间 (ms)
#define LONG_PRESS_TIME_MS      1000 // 长按判定时间 (ms)
#define DOUBLE_CLICK_TIME_MS    300  // 双击间隔时间 (ms)

// --- 新增的枚举和变量 ---
// 定义按钮状态机的状态
typedef enum {
    BUTTON_STATE_IDLE,          // 空闲状态
    BUTTON_STATE_PRESSED,       // 已按下，等待消抖
    BUTTON_STATE_WAIT_RELEASE,  // 等待释放 (已触发短按或长按)
    BUTTON_STATE_WAIT_DOUBLE,   // 等待双击的第二次按下
} ButtonState;

static ButtonState current_button_state = BUTTON_STATE_IDLE; // 当前按键状态
static TickType_t last_press_time = 0; // 上次按键按下的时间戳

// ======================= 修改点 1: 添加函数前向声明 =======================
// 告诉编译器后面会有一个叫 send_button_event 的函数，这样在 button_scan_task 中就可以正常调用它了
static void send_button_event(EventType_t event_type);
// =======================================================================


/**
 * @brief 发送按键事件的辅助函数
 * 
 * @param event_type 要发送的事件类型
 */
// ======================= 修改点 2: 修正函数参数的类型 =======================
static void send_button_event(EventType_t event_type) // <-- 这里从 AppEventType_t 改为 EventType_t
// =======================================================================
{
    AppEvent_t event_msg = {
        .event_type = event_type,
        .p_data = NULL,
        .data_len = 0
    };

    QueueHandle_t queue = get_event_queue();
    if (queue != NULL) {
        if (xQueueSend(queue, &event_msg, 0) == pdPASS) {
            ESP_LOGI(TAG, "成功发送事件: %d", event_type);
        } else {
            ESP_LOGW(TAG, "发送事件失败，队列已满: %d", event_type);
        }
    } else {
        ESP_LOGE(TAG, "无法获取事件队列句柄");
    }
}


static void button_scan_task(void *pvParameters)
{
    ESP_LOGI(TAG, "按键扫描任务已启动 (增强版)");

    while(1) {
        bool is_pressed = bsp_button_is_pressed();
        TickType_t current_tick = xTaskGetTickCount();

        switch (current_button_state) {
            case BUTTON_STATE_IDLE:
                // 空闲状态：等待按键按下
                if (is_pressed) {
                    last_press_time = current_tick; // 记录按下时间
                    current_button_state = BUTTON_STATE_PRESSED;
                }
                break;

            case BUTTON_STATE_PRESSED:
                // 已按下状态：进行消抖并判断是长按还是短按
                if (is_pressed) {
                    // 检查是否达到长按时间
                    if ((current_tick - last_press_time) >= pdMS_TO_TICKS(LONG_PRESS_TIME_MS)) {
                        send_button_event(EVENT_BUTTON_LONG_PRESS);
                        current_button_state = BUTTON_STATE_WAIT_RELEASE; // 进入等待释放状态
                    }
                } else {
                    // 按下后短时间内释放，说明不是长按，可能是单击或双击的第一次
                    // 进行防抖判断
                    if ((current_tick - last_press_time) >= pdMS_TO_TICKS(DEBOUNCE_TIME_MS)) {
                        last_press_time = current_tick; // 更新时间戳，用于双击计时
                        current_button_state = BUTTON_STATE_WAIT_DOUBLE;
                    } else {
                        // 按下时间太短，认为是抖动，忽略
                        current_button_state = BUTTON_STATE_IDLE;
                    }
                }
                break;

            case BUTTON_STATE_WAIT_DOUBLE:
                // 等待双击状态：在指定时间内等待第二次按下或超时
                if (is_pressed) {
                    // 在双击间隔内被再次按下，判定为双击
                    send_button_event(EVENT_BUTTON_DOUBLE_CLICK);
                    current_button_state = BUTTON_STATE_WAIT_RELEASE; // 进入等待释放状态
                } else {
                    // 检查是否超过了双击等待时间
                    if ((current_tick - last_press_time) >= pdMS_TO_TICKS(DOUBLE_CLICK_TIME_MS)) {
                        // 超时未按下，判定为单击
                        send_button_event(EVENT_BUTTON_SHORT_PRESS);
                        current_button_state = BUTTON_STATE_IDLE; // 返回空闲状态
                    }
                }
                break;
            
            case BUTTON_STATE_WAIT_RELEASE:
                // 等待释放状态：在触发长按或双击后，必须等待按键释放才能进行下一次检测
                if (!is_pressed) {
                    current_button_state = BUTTON_STATE_IDLE;
                }
                break;
        }

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