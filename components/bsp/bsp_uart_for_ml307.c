#include "bsp_uart_for_ml307.h"
#include "esp_log.h"

static const char *TAG = "BSP_ML307_UART";

esp_err_t bsp_uart_for_ml307_init(void)
{
    // 1. 配置UART参数
    uart_config_t uart_config = {
        .baud_rate = ML307_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // 2. 安装UART驱动
    esp_err_t ret = uart_driver_install(ML307_UART_PORT, ML307_UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART驱动安装失败, 错误码: %d", ret);
        return ret;
    }

    // 3. 设置UART参数
    ret = uart_param_config(ML307_UART_PORT, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART参数配置失败, 错误码: %d", ret);
        return ret;
    }

    // 4. 设置UART引脚
    ret = uart_set_pin(ML307_UART_PORT, ML307_UART_TX_PIN, ML307_UART_RX_PIN, ML307_UART_RTS_PIN, ML307_UART_CTS_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART引脚设置失败, 错误码: %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "ML307 UART初始化成功! Port: %d, Baud: %d", ML307_UART_PORT, ML307_UART_BAUD_RATE);
    return ESP_OK;
}