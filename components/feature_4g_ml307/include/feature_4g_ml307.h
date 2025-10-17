#ifndef FEATURE_4G_ML307_H
#define FEATURE_4G_ML307_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// 定义4G模块可以处理的事件类型 (新增TCP/UDP测试)
typedef enum {
    EVENT_4G_PERFORM_HTTP_TEST,
    EVENT_4G_PERFORM_MQTT_TEST,
    EVENT_4G_PERFORM_WEBSOCKET_TEST,
    EVENT_4G_PERFORM_TCP_TEST,
    EVENT_4G_PERFORM_UDP_TEST,
    // 新增：发送MQTT消息事件
    EVENT_4G_PUBLISH_MQTT_MESSAGE,
} Feature4GEventType_t;

// 定义事件消息结构体
// 使用union来为不同事件类型传递不同的数据，节省内存
typedef struct {
    Feature4GEventType_t event_type;
    union {
        struct {
            char topic[64];
            char payload[256];
        } mqtt_publish;
        // 未来可以为其他事件添加数据结构
    } data;
} Feature4GEvent_t;


/**
 * @brief 启动4G模块的核心任务
 *
 * 该函数会创建4G模块专用的消息队列和处理任务。
 * 必须在系统初始化时调用一次。
 *
 * @return pdPASS表示成功, 否则表示失败
 */
BaseType_t feature_4g_task_start(void);

/**
 * @brief 发送一个事件到4G模块的任务队列
 *
 * 这是一个非阻塞函数。它将请求打包成一个事件，发送给4G任务去处理。
 *
 * @param event 指向要发送的事件结构体的指针
 * @return pdPASS表示成功发送, errQUEUE_FULL表示队列已满
 */
BaseType_t feature_4g_send_event(const Feature4GEvent_t* event);

#ifdef __cplusplus
}
#endif

#endif // FEATURE_4G_ML307_H