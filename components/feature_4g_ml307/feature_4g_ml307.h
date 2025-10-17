#ifndef FEATURE_4G_ML307_H
#define FEATURE_4G_ML307_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stddef.h> // For size_t
#include <stdint.h> // For uint8_t

#ifdef __cplusplus
extern "C" {
#endif

// 定义4G模块可以处理的事件类型
typedef enum {
    // 保留的其他事件类型
    EVENT_4G_PERFORM_HTTP_TEST,
    EVENT_4G_PERFORM_MQTT_TEST,

    // --- 新增的WebSocket专属事件 ---
    /**
     * @brief 请求连接到WebSocket服务器.
     * 需要在 data.ws_connect.url 中提供服务器地址.
     */
    EVENT_4G_WEBSOCKET_CONNECT,

    /**
     * @brief 发送文本消息.
     * 需要在 data.ws_send_text.text 中提供字符串.
     */
    EVENT_4G_WEBSOCKET_SEND_TEXT,

    /**
     * @brief 发送二进制数据 (例如音频流).
     * 需要在 data.ws_send_binary 中提供数据指针和长度.
     */
    EVENT_4G_WEBSOCKET_SEND_BINARY,

    /**
     * @brief 主动断开WebSocket连接.
     */
    EVENT_4G_WEBSOCKET_DISCONNECT,
    
} Feature4GEventType_t;

// 定义事件消息结构体
// 使用union为不同事件传递不同数据
typedef struct {
    Feature4GEventType_t event_type;
    union {
        // WebSocket连接事件的数据
        struct {
            char url[128];
        } ws_connect;
        
        // WebSocket发送文本事件的数据
        struct {
            char text[256]; // 根据您的最大文本长度调整
        } ws_send_text;

        // WebSocket发送二进制事件的数据 (适用于音频流)
        // 注意：为避免栈拷贝大量数据，这里使用指针。
        // 调用者必须保证在4G任务处理完该事件前，该指针指向的内存是有效的！
        struct {
            const uint8_t* buffer;
            size_t length;
        } ws_send_binary;

        // 未来可以为其他事件添加数据结构
    } data;
} Feature4GEvent_t;

/**
 * @brief 初始化4G模块。
 * @details
 * 这是一个阻塞函数，应该在系统启动早期调用。
 * 它会完成以下工作：
 * 1. 创建内部消息队列。
 * 2. 从BSP获取硬件配置。
 * 3. 检测并初始化ML307模组。
 * 4. 阻塞等待，直到模组注网成功。
 * 只有当此函数返回 pdPASS 时，才能保证4G模块已准备就绪。
 *
 * @return pdPASS 表示初始化和网络连接成功，pdFAIL 表示失败。
 */
BaseType_t feature_4g_init(void);

/**
 * @brief 启动4G模块的核心事件处理任务。
 * @details
 * 必须在 feature_4g_init() 成功之后调用。
 * 此函数会创建一个后台任务，该任务会持续等待并处理通过 feature_4g_send_event() 发送的事件。
 *
 * @return pdPASS 表示任务创建成功，pdFAIL 表示失败。
 */
BaseType_t feature_4g_task_start(void);

/**
 * @brief 发送一个事件到4G模块的任务队列。
 * @details
 * 这是一个非阻塞函数，可以安全地在任何任务（包括中断服务程序，如果使用FromISR版本）中调用。
 *
 * @param event 指向要发送的事件结构体的指针。
 * @return pdPASS 表示事件成功入队，pdFAIL 表示队列已满或未初始化。
 */
BaseType_t feature_4g_send_event(const Feature4GEvent_t* event);

#ifdef __cplusplus
}
#endif

#endif // FEATURE_4G_ML307_H