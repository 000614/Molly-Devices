#include "feature_motor.h"
#include "bsp_motor.h" // 包含新的驱动层头文件
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"

static const char* TAG = "motor_module";

// =======================================================
//  1. 模块硬件配置
//  这里是震动马达硬件配置的唯一来源。
// =======================================================
#define MADA_GPIO           GPIO_NUM_18
#define MADA_LEDC_CHANNEL   LEDC_CHANNEL_0
#define MADA_CONTROL_MODE   MADA_CONTROL_DIGITAL // 可切换为 MADA_CONTROL_PWM

// 内部状态变量
static bool          g_is_vibrating = false;
static TimerHandle_t g_timer = NULL;
static bool          g_is_initialized = false;

// 定时器回调函数
static void timer_callback(TimerHandle_t timer);

esp_err_t mada_initialize(void) {
    if (g_is_initialized) {
        ESP_LOGW(TAG, "Mada module already initialized.");
        return ESP_OK;
    }

    // 1. 配置并初始化底层驱动
    mada_driver_config_t driver_config = {
        .pin = MADA_GPIO,
        .mode = MADA_CONTROL_MODE,
        .ledc_channel = MADA_LEDC_CHANNEL
    };
    esp_err_t ret = mada_driver_init(&driver_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mada driver");
        return ret;
    }

    // 2. 创建用于定时震动的软件定时器
    g_timer = xTimerCreate("mada_timer", pdMS_TO_TICKS(100), pdFALSE, NULL, timer_callback);
    if (g_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timer");
        mada_driver_deinit(); // 清理已初始化的驱动
        return ESP_FAIL;
    }
    
    g_is_initialized = true;
    ESP_LOGI(TAG, "Vibration motor module initialized.");
    return ESP_OK;
}

void mada_deinitialize() {
    if (!g_is_initialized) return;
    
    mada_stop();
    if (g_timer != NULL) {
        xTimerDelete(g_timer, portMAX_DELAY);
        g_timer = NULL;
    }
    
    mada_driver_deinit(); // 反初始化底层驱动
    
    g_is_initialized = false;
    ESP_LOGI(TAG, "Vibration motor module deinitialized.");
}

void mada_stop() {
    if (!g_is_initialized || !g_is_vibrating) return;

    mada_driver_off(); // 调用驱动层函数
    g_is_vibrating = false;

    if (g_timer != NULL) {
        xTimerStop(g_timer, 0);
    }
}

void mada_set_intensity(uint8_t intensity) {
    if (!g_is_initialized) return;
    // 直接将强度设置请求传递给驱动层
    mada_driver_set_intensity(intensity);
}

void mada_vibrate(uint32_t duration_ms) {
    if (!g_is_initialized) return;
    
    mada_driver_on(); // 调用驱动层函数
    g_is_vibrating = true;

    if (g_timer != NULL) {
        xTimerChangePeriod(g_timer, pdMS_TO_TICKS(duration_ms), 0);
        xTimerStart(g_timer, 0);
    }
}

void mada_continuous_vibrate() {
    if (!g_is_initialized) return;
    
    // 停止可能正在运行的定时器
    if (g_timer != NULL) {
        xTimerStop(g_timer, 0);
    }
    
    mada_driver_on(); // 调用驱动层函数
    g_is_vibrating = true;
}

void mada_short_vibrate() {
    mada_vibrate(100);
}

void mada_long_vibrate() {
    mada_vibrate(500);
}

bool mada_is_vibrating() {
    return g_is_vibrating;
}

static void timer_callback(TimerHandle_t timer) {
    // 定时器到期后，调用停止函数
    mada_stop();
}