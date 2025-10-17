#ifndef BSP_UART_FOR_ML307_H
#define BSP_UART_FOR_ML307_H

#include "driver/uart.h"
#include "driver/gpio.h" 

#ifdef __cplusplus
extern "C" {
#endif

// 定义ML307使用的UART端口和引脚
#define ML307_UART_PORT      UART_NUM_2
#define ML307_UART_TX_PIN    GPIO_NUM_14 // 根据您的实际硬件连接修改
#define ML307_UART_RX_PIN    GPIO_NUM_13 // 根据您的实际硬件连接修改
#define ML307_UART_RTS_PIN   (UART_PIN_NO_CHANGE)
#define ML307_UART_CTS_PIN   (UART_PIN_NO_CHANGE)

// 定义UART缓冲区大小和波特率
#define ML307_UART_BAUD_RATE 921600
#define ML307_UART_BUF_SIZE  (2048)

/**
 * @brief 初始化用于ML307模块的UART外设
 *
 * @return esp_err_t ESP_OK表示成功, 否则失败
 */
esp_err_t bsp_uart_for_ml307_init(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_UART_FOR_ML307_H