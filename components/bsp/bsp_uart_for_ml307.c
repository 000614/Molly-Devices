// bsp_uart_for_ml307.c

#include "bsp_uart_for_ml307.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char* TAG = "BSP_ML307";


#define MODEM_UART_TX_PIN   GPIO_NUM_14
#define MODEM_UART_RX_PIN   GPIO_NUM_13
#define MODEM_PWRKEY_PIN    GPIO_NUM_15
#define MODEM_BAUD_RATE     115200
// ====================================================================

/**
 * @brief 获取ML307模块的板级硬件配置的实现
 */
void bsp_ml307_get_config(bsp_ml307_config_t *config)
{
    if (config != NULL) {
        config->uart_tx_pin = MODEM_UART_TX_PIN;
        config->uart_rx_pin = MODEM_UART_RX_PIN;
        config->pwrkey_pin  = MODEM_PWRKEY_PIN;
        config->baud_rate   = MODEM_BAUD_RATE;
    }
}

