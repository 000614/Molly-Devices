// bsp_uart_for_ml307.h

#ifndef BSP_UART_FOR_ML307_H
#define BSP_UART_FOR_ML307_H

#include "driver/gpio.h" // 引入 gpio_num_t 类型

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 封装ML307模块所需硬件配置的结构体
 */
typedef struct {
    gpio_num_t uart_tx_pin; ///< UART发送引脚
    gpio_num_t uart_rx_pin; ///< UART接收引脚
    gpio_num_t pwrkey_pin;  ///< 电源按键引脚
    int baud_rate;          ///< UART通信波特率
} bsp_ml307_config_t;

/**
 * @brief 获取ML307模块的板级硬件配置
 *
 * @details 此函数从BSP层读取预定义的硬件参数，并填充到传入的配置结构体中。
 *          应用层代码通过调用此函数来获取硬件信息，从而实现与具体硬件解耦。
 *
 * @param[out] config 一个指向 bsp_ml307_config_t 结构体的指针。函数将把获取到的配置写入此结构体。
 */
void bsp_ml307_get_config(bsp_ml307_config_t *config);



#ifdef __cplusplus
}
#endif

#endif // BSP_UART_FOR_ML307_H