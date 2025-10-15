#include "bsp_button.h"
#include "driver/gpio.h"
#include "soc/gpio_num.h"

#define BUTTON_GPIO GPIO_NUM_47

esp_err_t bsp_button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // 使用内部上拉电阻
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io_conf);
}

bool bsp_button_is_pressed(void)
{
    // 因为使用了上拉电阻，所以当按钮按下时，GPIO电平为低(0)
    return (gpio_get_level(BUTTON_GPIO) == 0);
}
