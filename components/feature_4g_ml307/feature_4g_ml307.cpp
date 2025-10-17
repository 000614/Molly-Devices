#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <memory> // for std::unique_ptr
#include <string> // for std::string

// --- 核心依赖：新的4G模块C++库 ---
#include "at_modem.h"

// --- 本模块的C风格头文件 ---
#include "feature_4g_ml307.h"

// --- BSP层头文件 ---
// 注意：新库自动处理UART初始化，我们只需要提供引脚号
// #include "bsp_uart_for_ml307.h" // 不再需要手动初始化UART

#define TAG "FEATURE_4G"
#define FEATURE_4G_TASK_STACK_SIZE (1024 * 8)
#define FEATURE_4G_TASK_PRIORITY   (5)
#define FEATURE_4G_QUEUE_LENGTH    (10) // 队列可以适当加长

// --- 硬件引脚定义 ---
#define MODEM_UART_TX_PIN   GPIO_NUM_14 // 根据您的硬件修改
#define MODEM_UART_RX_PIN   GPIO_NUM_13 // 根据您的硬件修改
#define MODEM_PWRKEY_PIN    GPIO_NUM_15 // 根据您的硬件修改
#define MODEM_BAUD_RATE     921600

// 模块内部的静态变量
static QueueHandle_t s_feature_4g_queue = NULL;

// --- 私有的事件处理函数 (C++实现) ---

static void prv_handle_http_test(AtModem& modem) {
    ESP_LOGI(TAG, "开始处理HTTP测试...");
    auto http = modem.CreateHttp(0);
    if (!http) {
        ESP_LOGE(TAG, "创建HTTP客户端失败");
        return;
    }
    
    http->SetHeader("User-Agent", "MyESP32Project/3.0");
    http->SetTimeout(10000);

    if (http->Open("GET", "https://httpbin.org/json")) {
        ESP_LOGI(TAG, "HTTP状态码: %d", http->GetStatusCode());
        std::string response = http->ReadAll();
        ESP_LOGI(TAG, "HTTP响应体: %s", response.c_str());
        http->Close();
    } else {
        ESP_LOGE(TAG, "HTTP请求失败");
    }
    ESP_LOGI(TAG, "HTTP测试处理完毕!");
    // http unique_ptr 在函数结束时自动释放
}

static void prv_handle_mqtt_test(AtModem& modem) {
    ESP_LOGI(TAG, "开始处理MQTT测试...");
    auto mqtt = modem.CreateMqtt(0);
    if (!mqtt) {
        ESP_LOGE(TAG, "创建MQTT客户端失败");
        return;
    }
    
    mqtt->OnConnected([]() { ESP_LOGI(TAG, "MQTT连接成功"); });
    mqtt->OnDisconnected([]() { ESP_LOGI(TAG, "MQTT连接断开"); });
    mqtt->OnMessage([](const std::string& topic, const std::string& payload) {
        ESP_LOGI(TAG, "收到MQTT消息 [%s]: %s", topic.c_str(), payload.c_str());
    });

    if (mqtt->Connect("broker.emqx.io", 1883, "esp32_client_v3", "", "")) {
        mqtt->Subscribe("test/esp32/message");
        mqtt->Publish("test/esp32/hello", "Hello from AtModem v3.0!");
        vTaskDelay(pdMS_TO_TICKS(5000)); // 等待接收消息
        mqtt->Disconnect();
    } else {
        ESP_LOGE(TAG, "MQTT连接失败");
    }
    ESP_LOGI(TAG, "MQTT测试处理完毕!");
}

static void prv_handle_websocket_test(AtModem& modem) {
    ESP_LOGI(TAG, "开始处理WebSocket测试...");
    auto ws = modem.CreateWebSocket(0);
    if (!ws) {
        ESP_LOGE(TAG, "创建WebSocket客户端失败");
        return;
    }

    ws->OnConnected([]() { ESP_LOGI(TAG, "WebSocket连接成功"); });
    ws->OnData([](const char* data, size_t length, bool binary) {
        ESP_LOGI(TAG, "收到WebSocket数据: %.*s", (int)length, data);
    });
    ws->OnDisconnected([]() { ESP_LOGI(TAG, "WebSocket连接断开"); });

    if (ws->Connect("wss://echo.websocket.org/")) {
        ws->Send("Hello from ESP32 via ML307!");
        vTaskDelay(pdMS_TO_TICKS(2000));
        ws->Close();
    } else {
        ESP_LOGE(TAG, "WebSocket连接失败");
    }
    ESP_LOGI(TAG, "WebSocket测试处理完毕!");
}

// --- 核心处理任务 (C++代码) ---

static void feature_4g_handler_task(void *pvParameters)
{
    ESP_LOGI(TAG, "4G处理任务已启动");

    // 1. 使用新API自动检测并初始化模组
    auto modem = AtModem::Detect(MODEM_UART_RX_PIN, MODEM_UART_TX_PIN, MODEM_PWRKEY_PIN, MODEM_BAUD_RATE);
    
    if (!modem) {
        ESP_LOGE(TAG, "模组检测失败，任务将自毁");
        vTaskDelete(NULL);
        return;
    }

    // 2. 设置网络状态回调
    modem->OnNetworkStateChanged([](bool ready) {
        ESP_LOGI(TAG, "网络状态变化: %s", ready ? "已连接" : "已断开");
    });
    
    // 3. 等待网络就绪，并处理可能的错误
    ESP_LOGI(TAG, "等待网络就绪...");
    NetworkStatus status = modem->WaitForNetworkReady(60000); // 等待60秒
    if (status != NetworkStatus::Ready) {
        ESP_LOGE(TAG, "网络连接失败，状态码: %d。任务将自毁", (int)status);
        vTaskDelete(NULL);
        return;
    }
    
    // 4. 打印模组信息
    ESP_LOGI(TAG, "网络已就绪!");
    ESP_LOGI(TAG, "模组版本: %s", modem->GetModuleRevision().c_str());
    ESP_LOGI(TAG, "IMEI: %s", modem->GetImei().c_str());
    ESP_LOGI(TAG, "ICCID: %s", modem->GetIccid().c_str());
    ESP_LOGI(TAG, "运营商: %s", modem->GetCarrierName().c_str());
    ESP_LOGI(TAG, "信号强度: %d", modem->GetCsq());

    Feature4GEvent_t received_event;
    
    // 5. 进入主循环，阻塞等待来自C代码的事件消息
    while(1) {
        if (xQueueReceive(s_feature_4g_queue, &received_event, portMAX_DELAY) == pdPASS) {
            
            switch (received_event.event_type) {
                case EVENT_4G_PERFORM_HTTP_TEST:
                    prv_handle_http_test(*modem); // 传递modem对象的引用
                    break;
                
                case EVENT_4G_PERFORM_MQTT_TEST:
                    prv_handle_mqtt_test(*modem);
                    break;
                
                case EVENT_4G_PERFORM_WEBSOCKET_TEST:
                    prv_handle_websocket_test(*modem);
                    break;
                
                // 可以在这里添加TCP/UDP测试的处理
                case EVENT_4G_PERFORM_TCP_TEST:
                    ESP_LOGI(TAG, "TCP测试待实现");
                    break;
                
                case EVENT_4G_PERFORM_UDP_TEST:
                    ESP_LOGI(TAG, "UDP测试待实现");
                    break;
                
                default:
                    ESP_LOGW(TAG, "收到未知的4G事件类型: %d", received_event.event_type);
                    break;
            }
        }
    }
}


// --- 公共API函数实现 (C风格接口，供外部C文件调用) ---


extern "C" BaseType_t feature_4g_task_start(void)
{
    // ***** 添加日志 *****
    ESP_LOGI(TAG, "feature_4g_task_start() 函数被调用!");

    if (s_feature_4g_queue != NULL) {
        ESP_LOGW(TAG, "4G任务已经启动，无需重复创建");
        return pdPASS;
    }
    
    s_feature_4g_queue = xQueueCreate(FEATURE_4G_QUEUE_LENGTH, sizeof(Feature4GEvent_t));
    if (s_feature_4g_queue == NULL) {
        // ***** 添加日志 *****
        ESP_LOGE(TAG, "关键错误：4G消息队列创建失败! (内存不足?)");
        return pdFAIL;
    } else {
        // ***** 添加日志 *****
        ESP_LOGI(TAG, "4G消息队列创建成功! 句柄: %p", s_feature_4g_queue);
    }

    BaseType_t status = xTaskCreate(
        feature_4g_handler_task,
        "feature_4g_task",
        FEATURE_4G_TASK_STACK_SIZE,
        NULL,
        FEATURE_4G_TASK_PRIORITY,
        NULL
    );

    if (status != pdPASS) {
        ESP_LOGE(TAG, "关键错误：4G核心任务创建失败!");
        vQueueDelete(s_feature_4g_queue);
        s_feature_4g_queue = NULL;
    } else {
        // ***** 添加日志 *****
        ESP_LOGI(TAG, "4G核心任务创建成功!");
    }
    return status;
}



extern "C" BaseType_t feature_4g_send_event(const Feature4GEvent_t* event)
{
    if (s_feature_4g_queue == NULL) {
        // ***** 添加日志 *****
        ESP_LOGE(TAG, "发送失败，因为4G消息队列句柄为NULL!");
        return pdFAIL;
    }

    BaseType_t send_status = xQueueSend(s_feature_4g_queue, event, 0);
    if (send_status != pdPASS) {
        // ***** 添加日志 *****
        ESP_LOGW(TAG, "xQueueSend失败，错误码: %d (可能队列已满)", send_status);
    }
    return send_status;
}