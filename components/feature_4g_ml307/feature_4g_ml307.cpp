#include "freertos/FreeRTOS.h"
#include "freertos/task.h" // For TaskHandle_t
#include "freertos/queue.h"
#include "esp_log.h"
#include <memory> // for std::unique_ptr
#include <string> // for std::string

#include "at_modem.h"
#include "feature_4g_ml307.h"
#include "bsp_uart_for_ml307.h"

#define TAG "FEATURE_4G"
#define FEATURE_4G_TASK_STACK_SIZE (1024 * 8)
#define FEATURE_4G_TASK_PRIORITY   (5)
#define FEATURE_4G_QUEUE_LENGTH    (10)

// --- 模块内部的静态全局变量 ---
static QueueHandle_t s_feature_4g_queue = NULL;
static TaskHandle_t s_feature_4g_task_handle = NULL;
static std::unique_ptr<AtModem> s_modem = nullptr; // 使用智能指针管理modem对象生命周期
static bool s_is_initialized = false;             // 初始化成功标志


static void prv_handle_websocket_connect(AtModem& modem, const char* url) {
    ESP_LOGI(TAG, "开始处理WebSocket连接请求，目标URL: %s", url);
    auto ws = modem.CreateWebSocket(0); // 创建WebSocket客户端实例
    if (!ws) {
        ESP_LOGE(TAG, "创建WebSocket客户端失败");
        return;
    }
    // 设置请求头
    // ws->SetHeader("Protocol-Version", "3");
    // 设置回调函数
    ws->OnConnected([]() { ESP_LOGI(TAG, "WebSocket连接成功"); });

    ws->OnData([](const char* data, size_t length, bool binary) {
        ESP_LOGI(TAG, "收到WebSocket数据: %.*s", (int)length, data);
    });
    ws->OnDisconnected([]() { ESP_LOGI(TAG, "WebSocket连接断开"); });
    ws->OnError([](int error) {
        ESP_LOGE(TAG, "WebSocket 错误: %d", error);
    });


    // 使用从事件中传入的URL，而不是硬编码的URL
    if (ws->Connect(url)) {
        // 发送消息
        for (int i = 0; i < 5; i++) {
            std::string message = "{\"type\": \"ping\", \"id\": " + std::to_string(i) + "}";
            ws->Send(message);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        
        ws->Close();
    } else {
        ESP_LOGE(TAG, "WebSocket 连接失败");
    }
    
    
}

static void feature_4g_handler_task(void *pvParameters)
{
    ESP_LOGI(TAG, "4G事件处理任务已启动，进入主循环。");
    
    Feature4GEvent_t received_event;
    
    // 任务现在直接进入主循环，等待事件
    while(1) {
        if (xQueueReceive(s_feature_4g_queue, &received_event, portMAX_DELAY) == pdPASS) {
            
            // 确保s_modem是有效的
            if (!s_modem) {
                ESP_LOGE(TAG, "严重错误: Modem对象为空，无法处理事件！");
                continue;
            }

            switch (received_event.event_type) {
                case EVENT_4G_WEBSOCKET_CONNECT:
                    // 使用静态的modem对象
                    prv_handle_websocket_connect(*s_modem, received_event.data.ws_connect.url);
                    break;
                
                default:
                    ESP_LOGW(TAG, "收到未知的4G事件类型: %d", received_event.event_type);
                    break;
            }
        }
    }
}


// --- 公共API函数实现 (C风格接口) ---

/**
 * @brief [新增] 初始化函数
 */
extern "C" BaseType_t feature_4g_init(void)
{
    if (s_is_initialized) {
        ESP_LOGW(TAG, "feature_4g_init() 重复调用，已忽略。");
        return pdPASS;
    }

    ESP_LOGI(TAG, "开始初始化4G模块...");

    // 1. 创建消息队列
    s_feature_4g_queue = xQueueCreate(FEATURE_4G_QUEUE_LENGTH, sizeof(Feature4GEvent_t));
    if (s_feature_4g_queue == NULL) {
        ESP_LOGE(TAG, "关键错误：4G消息队列创建失败!");
        return pdFAIL;
    }

    // 2. 获取BSP硬件配置
    bsp_ml307_config_t modem_config;
    bsp_ml307_get_config(&modem_config);

    
    ESP_LOGI(TAG, "使用BSP配置: RX=%d, TX=%d, PWRKEY=%d, BAUD=%d",
             modem_config.uart_rx_pin, modem_config.uart_tx_pin,
             modem_config.pwrkey_pin, modem_config.baud_rate);

    // 3. 自动检测并初始化模组
    s_modem = AtModem::Detect(modem_config.uart_rx_pin, 
                              modem_config.uart_tx_pin, 
                              modem_config.pwrkey_pin, 
                              modem_config.baud_rate);
    
    if (!s_modem) {
        ESP_LOGE(TAG, "ML307模组检测失败!");
        vQueueDelete(s_feature_4g_queue);
        s_feature_4g_queue = NULL;
        return pdFAIL;
    }

    // 4. 设置网络状态回调
    s_modem->OnNetworkStateChanged([](bool ready) {
        ESP_LOGI(TAG, "网络状态变化: %s", ready ? "已连接" : "已断开");
    });
    
    // 5. 阻塞等待网络就绪
    ESP_LOGI(TAG, "等待网络就绪 (最长60秒)...");
    NetworkStatus status = s_modem->WaitForNetworkReady(60000);
    if (status != NetworkStatus::Ready) {
        ESP_LOGE(TAG, "网络连接失败，状态码: %d", (int)status);
        s_modem.reset(); // 释放modem对象
        vQueueDelete(s_feature_4g_queue);
        s_feature_4g_queue = NULL;
        return pdFAIL;
    }
    
    // 6. 打印模组信息
    ESP_LOGI(TAG, "网络已就绪!");
    ESP_LOGI(TAG, "模组版本: %s", s_modem->GetModuleRevision().c_str());
    ESP_LOGI(TAG, "IMEI: %s", s_modem->GetImei().c_str());
    ESP_LOGI(TAG, "ICCID: %s", s_modem->GetIccid().c_str());
    ESP_LOGI(TAG, "运营商: %s", s_modem->GetCarrierName().c_str());
    ESP_LOGI(TAG, "信号强度: %d", s_modem->GetCsq());

    s_is_initialized = true;
    ESP_LOGI(TAG, "4G模块初始化成功，已准备好启动任务。");

    return pdPASS;
}

/**
 * @brief [改造] 任务启动函数
 */
extern "C" BaseType_t feature_4g_task_start(void)
{
    if (!s_is_initialized) {
        ESP_LOGE(TAG, "启动失败，请先调用 feature_4g_init() 并确保其成功返回。");
        return pdFAIL;
    }

    if (s_feature_4g_task_handle != NULL) {
        ESP_LOGW(TAG, "4G任务已经启动，无需重复创建");
        return pdPASS;
    }

    BaseType_t status = xTaskCreate(
        feature_4g_handler_task,
        "feature_4g_task",
        FEATURE_4G_TASK_STACK_SIZE,
        NULL,
        FEATURE_4G_TASK_PRIORITY,
        &s_feature_4g_task_handle // 保存任务句柄
    );

    if (status != pdPASS) {
        ESP_LOGE(TAG, "关键错误：4G核心任务创建失败!");
        s_feature_4g_task_handle = NULL;
    } else {
        ESP_LOGI(TAG, "4G核心任务创建成功!");
    }
    
    return status;
}

/**
 * @brief [改造] 事件发送函数 (增加检查)
 */
extern "C" BaseType_t feature_4g_send_event(const Feature4GEvent_t* event)
{
    if (!s_is_initialized || s_feature_4g_queue == NULL) {
        ESP_LOGE(TAG, "发送失败，4G模块未初始化或队列为空!");
        return pdFAIL;
    }

    if (s_feature_4g_task_handle == NULL) {
        ESP_LOGW(TAG, "警告：正在向一个尚未启动的任务发送事件！请先调用 feature_4g_task_start()。");
    }

    if (xQueueSend(s_feature_4g_queue, event, 0) != pdPASS) {
        ESP_LOGW(TAG, "发送到4G队列失败，可能队列已满。");
        return pdFAIL;
    }
    
    return pdPASS;
}
