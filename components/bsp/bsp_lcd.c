#include "bsp_lcd.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <esp_lcd_gc9a01.h>

#define TAG "BSP_LCD"

// --- 硬件配置 (私有) ---
#define LCD_HOST            SPI3_HOST
#define PIN_NUM_LCD_CS      (-1)
#define PIN_NUM_LCD_PCLK    (GPIO_NUM_8)
#define PIN_NUM_LCD_DATA0   (GPIO_NUM_9)
#define PIN_NUM_LCD_RST     (GPIO_NUM_10)
#define PIN_NUM_LCD_DC      (GPIO_NUM_11)

// --- 屏幕物理参数 (私有) ---
#define LCD_H_RES           (240)
#define LCD_V_RES           (240)
#define LCD_BIT_PER_PIXEL   (16)

// 模块内部静态变量，外部无法访问
static esp_lcd_panel_io_handle_t s_io_handle = NULL;
static esp_lcd_panel_handle_t s_panel_handle = NULL;
static SemaphoreHandle_t s_dma_done_sem = NULL;

// [新增] 1. 定义一个互斥锁句柄，用于保护对LCD硬件的并发访问
static SemaphoreHandle_t s_lcd_mutex = NULL;

static uint8_t s_last_brightness = 255; 

// DMA 完成回调函数
static bool IRAM_ATTR lcd_trans_done_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    SemaphoreHandle_t dma_done_sem = (SemaphoreHandle_t)user_ctx;
    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR(dma_done_sem, &task_woken);
    return (task_woken == pdTRUE);
}


// [新增] 2. 创建一个内部的、不带锁的亮度设置函数
// 这样做的目的是为了避免在 set_power 函数中产生死锁（自己锁住自己）
static esp_err_t _bsp_lcd_set_brightness_internal(uint8_t brightness)
{
    // 当设置的亮度值大于0时，我们将其记录下来，以便开屏时恢复
    if (brightness > 0) {
        s_last_brightness = brightness;
    }
    
    // 0xc9 是 GC9A01 芯片 "Write Display Brightness" 的命令
    return esp_lcd_panel_io_tx_param(s_io_handle, 0xc9, (uint8_t[]){brightness}, 1);
}


// --- 公共 API 实现 ---

esp_err_t bsp_lcd_init(void)
{
    // [新增] 3. 在初始化函数的最开始，创建互斥锁
    s_lcd_mutex = xSemaphoreCreateMutex();
    if (s_lcd_mutex == NULL) {
        ESP_LOGE(TAG, "创建 LCD 互斥锁失败!");
        return ESP_FAIL;
    }

    s_dma_done_sem = xSemaphoreCreateBinary();
    if (s_dma_done_sem == NULL) {
        ESP_LOGE(TAG, "创建DMA信号量失败!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "SPI总线初始化中");
    const spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_LCD_DATA0,
        .miso_io_num = -1,
        .sclk_io_num = PIN_NUM_LCD_PCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (LCD_H_RES * LCD_V_RES * LCD_BIT_PER_PIXEL / 8),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "创建SPI面板IO句柄");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_NUM_LCD_CS,
        .dc_gpio_num = PIN_NUM_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = 60 * 1000 * 1000,
        .trans_queue_depth = 1,
        .on_color_trans_done = lcd_trans_done_cb,
        .user_ctx = s_dma_done_sem,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &s_io_handle));

    ESP_LOGI(TAG, "创建GC9A01面板驱动");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(s_io_handle, &panel_config, &s_panel_handle));
    
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));

    // 初始化最后，设置一个默认的亮度，确保背光开启
    bsp_lcd_set_brightness(255);

    ESP_LOGI(TAG, "GC9A01 LCD初始化完成");
    return ESP_OK;
}

uint16_t bsp_lcd_get_width(void)
{
    return LCD_H_RES;
}

uint16_t bsp_lcd_get_height(void)
{
    return LCD_V_RES;
}

// [修改] 4. 在 draw_bitmap 函数中加入互斥锁保护
esp_err_t bsp_lcd_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    esp_err_t ret = ESP_FAIL;
    // 获取锁，如果锁被其他任务占用，此任务将在此等待
    if (xSemaphoreTake(s_lcd_mutex, portMAX_DELAY) == pdTRUE) {
        // --- 临界区开始 ---
        ret = esp_lcd_panel_draw_bitmap(s_panel_handle, x_start, y_start, x_end, y_end, color_data);
        // --- 临界区结束 ---
        
        // 释放锁，让其他任务可以访问LCD
        xSemaphoreGive(s_lcd_mutex);
    }
    return ret;
}

// 这个函数操作的是DMA完成信号量，本身就是一种同步机制，不需要再加互斥锁
void bsp_lcd_wait_for_draw_done(void)
{
    xSemaphoreTake(s_dma_done_sem, portMAX_DELAY);
}

// [修改] 5. 在 set_power 函数中加入互斥锁保护
esp_err_t bsp_lcd_set_power(bool on)
{
    esp_err_t ret = ESP_FAIL;
    if (xSemaphoreTake(s_lcd_mutex, portMAX_DELAY) == pdTRUE) {
        // --- 临界区开始 ---
        if (on) {
            ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));
            ret = _bsp_lcd_set_brightness_internal(s_last_brightness); // 调用内部函数
        } else {
            ESP_ERROR_CHECK(_bsp_lcd_set_brightness_internal(0)); // 调用内部函数
            ret = esp_lcd_panel_disp_on_off(s_panel_handle, false);
        }
        // --- 临界区结束 ---
        xSemaphoreGive(s_lcd_mutex);
    }
    return ret;
}

// [修改] 6. 公开的 set_brightness 函数也需要互斥锁保护
esp_err_t bsp_lcd_set_brightness(uint8_t brightness)
{
    esp_err_t ret = ESP_FAIL;
    if (xSemaphoreTake(s_lcd_mutex, portMAX_DELAY) == pdTRUE) {
        // --- 临界区开始 ---
        ret = _bsp_lcd_set_brightness_internal(brightness);
        // --- 临界区结束 ---
        xSemaphoreGive(s_lcd_mutex);
    }
    return ret;
}