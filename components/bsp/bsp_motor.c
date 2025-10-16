#include "bsp_motor.h"
#include "esp_log.h"
#include "esp_check.h"

static const char* TAG = "mada_driver";

// 驱动内部状态
static struct {
    mada_driver_config_t config;
    bool is_initialized;
} s_driver_state = {
    .is_initialized = false
};

// PWM配置常量
#define PWM_FREQ_HZ         1000
#define PWM_RESOLUTION      LEDC_TIMER_8_BIT
#define PWM_TIMER           LEDC_TIMER_0
#define PWM_SPEED_MODE      LEDC_LOW_SPEED_MODE

// 内部函数声明
static esp_err_t initialize_pwm(void);
static esp_err_t initialize_digital(void);


esp_err_t mada_driver_init(const mada_driver_config_t* config) {
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Config cannot be null");
    if (s_driver_state.is_initialized) {
        ESP_LOGW(TAG, "Driver already initialized.");
        return ESP_OK;
    }

    s_driver_state.config = *config;

    if (s_driver_state.config.mode == MADA_CONTROL_PWM) {
        ESP_RETURN_ON_ERROR(initialize_pwm(), TAG, "Failed to initialize PWM");
    } else {
        ESP_RETURN_ON_ERROR(initialize_digital(), TAG, "Failed to initialize digital");
    }

    s_driver_state.is_initialized = true;
    ESP_LOGI(TAG, "Mada driver initialized on GPIO %d in %s mode", 
             s_driver_state.config.pin, 
             s_driver_state.config.mode == MADA_CONTROL_PWM ? "PWM" : "Digital");
    
    return ESP_OK;
}

void mada_driver_deinit(void) {
    if (!s_driver_state.is_initialized) return;
    
    mada_driver_off();
    // 可以添加 gpio_reset_pin 等清理操作
    s_driver_state.is_initialized = false;
    ESP_LOGI(TAG, "Mada driver deinitialized.");
}

void mada_driver_on(void) {
    if (!s_driver_state.is_initialized) return;

    if (s_driver_state.config.mode == MADA_CONTROL_PWM) {
        // 默认50%强度启动
        uint32_t duty = (50 * 255) / 100;
        ledc_set_duty(PWM_SPEED_MODE, s_driver_state.config.ledc_channel, duty);
        ledc_update_duty(PWM_SPEED_MODE, s_driver_state.config.ledc_channel);
    } else {
        gpio_set_level(s_driver_state.config.pin, 1);
    }
}

void mada_driver_off(void) {
    if (!s_driver_state.is_initialized) return;

    if (s_driver_state.config.mode == MADA_CONTROL_PWM) {
        ledc_set_duty(PWM_SPEED_MODE, s_driver_state.config.ledc_channel, 0);
        ledc_update_duty(PWM_SPEED_MODE, s_driver_state.config.ledc_channel);
    } else {
        gpio_set_level(s_driver_state.config.pin, 0);
    }
}

void mada_driver_set_intensity(uint8_t intensity) {
    if (!s_driver_state.is_initialized || s_driver_state.config.mode != MADA_CONTROL_PWM) return;
    
    if (intensity > 100) intensity = 100;

    uint32_t duty = (intensity * ((1 << PWM_RESOLUTION) - 1)) / 100;
    ledc_set_duty(PWM_SPEED_MODE, s_driver_state.config.ledc_channel, duty);
    ledc_update_duty(PWM_SPEED_MODE, s_driver_state.config.ledc_channel);
}

static esp_err_t initialize_pwm() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = PWM_SPEED_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num = PWM_TIMER,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&ledc_timer), TAG, "ledc_timer_config failed");

    ledc_channel_config_t ledc_channel = {
        .gpio_num = s_driver_state.config.pin,
        .speed_mode = PWM_SPEED_MODE,
        .channel = s_driver_state.config.ledc_channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    return ledc_channel_config(&ledc_channel);
}

static esp_err_t initialize_digital() {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << s_driver_state.config.pin),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "gpio_config failed");
    return gpio_set_level(s_driver_state.config.pin, 0);
}