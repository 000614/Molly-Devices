#include "bsp_lcd.h"

// =================================================================================================
// SECTION 1: COMMON SYSTEM INCLUDES
// =================================================================================================
#include <stdlib.h>
#include <sys/cdefs.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_check.h"

// =================================================================================================
// SECTION 2: INTERNAL GC9A01 DRIVER (PRIVATE IMPLEMENTATION)
// All declarations and implementations from the original GC9A01 driver are placed here.
// They are not visible outside of this file.
// =================================================================================================

#define GC9A01_TAG "gc9a01" // 为底层驱动日志定义独立的TAG

// --- Private Declarations (from original esp_lcd_gc9a01.h) ---

typedef struct {
    int cmd;
    const void *data;
    size_t data_bytes;
    unsigned int delay_ms;
} gc9a01_lcd_init_cmd_t;

typedef struct {
    const gc9a01_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
} gc9a01_vendor_config_t;

// 函数声明，确保 bsp_lcd_init 可以调用它
static esp_err_t esp_lcd_new_panel_gc9a01(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);


// --- Private Implementation (from original esp_lcd_gc9a01.c) ---

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val;
    uint8_t colmod_val;
    const gc9a01_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
} gc9a01_panel_t;

// 声明所有静态函数
static esp_err_t panel_gc9a01_del(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9a01_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9a01_init(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9a01_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_gc9a01_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_gc9a01_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_gc9a01_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_gc9a01_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_gc9a01_disp_on_off(esp_lcd_panel_t *panel, bool off);

static esp_err_t esp_lcd_new_panel_gc9a01(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    gc9a01_panel_t *gc9a01 = NULL;
    gpio_config_t io_conf = { 0 };

    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, GC9A01_TAG, "invalid argument");
    gc9a01 = (gc9a01_panel_t *)calloc(1, sizeof(gc9a01_panel_t));
    ESP_GOTO_ON_FALSE(gc9a01, ESP_ERR_NO_MEM, err, GC9A01_TAG, "no mem for gc9a01 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num;
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, GC9A01_TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_endian) {
    case LCD_RGB_ENDIAN_RGB:
        gc9a01->madctl_val = 0;
        break;
    case LCD_RGB_ENDIAN_BGR:
        gc9a01->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, GC9A01_TAG, "unsupported rgb endian");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 16:
        gc9a01->colmod_val = 0x55;
        gc9a01->fb_bits_per_pixel = 16;
        break;
    case 18:
        gc9a01->colmod_val = 0x66;
        gc9a01->fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, GC9A01_TAG, "unsupported pixel width");
        break;
    }

    gc9a01->io = io;
    gc9a01->reset_gpio_num = panel_dev_config->reset_gpio_num;
    gc9a01->reset_level = panel_dev_config->flags.reset_active_high;
    if (panel_dev_config->vendor_config) {
        gc9a01->init_cmds = ((gc9a01_vendor_config_t *)panel_dev_config->vendor_config)->init_cmds;
        gc9a01->init_cmds_size = ((gc9a01_vendor_config_t *)panel_dev_config->vendor_config)->init_cmds_size;
    }
    gc9a01->base.del = panel_gc9a01_del;
    gc9a01->base.reset = panel_gc9a01_reset;
    gc9a01->base.init = panel_gc9a01_init;
    gc9a01->base.draw_bitmap = panel_gc9a01_draw_bitmap;
    gc9a01->base.invert_color = panel_gc9a01_invert_color;
    gc9a01->base.set_gap = panel_gc9a01_set_gap;
    gc9a01->base.mirror = panel_gc9a01_mirror;
    gc9a01->base.swap_xy = panel_gc9a01_swap_xy;
    gc9a01->base.disp_on_off = panel_gc9a01_disp_on_off;
    *ret_panel = &(gc9a01->base);
    ESP_LOGD(GC9A01_TAG, "new gc9a01 panel @%p", gc9a01);
    return ESP_OK;

err:
    if (gc9a01) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(gc9a01);
    }
    return ret;
}

static esp_err_t panel_gc9a01_del(esp_lcd_panel_t *panel)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    if (gc9a01->reset_gpio_num >= 0) {
        gpio_reset_pin(gc9a01->reset_gpio_num);
    }
    ESP_LOGD(GC9A01_TAG, "del gc9a01 panel @%p", gc9a01);
    free(gc9a01);
    return ESP_OK;
}

static esp_err_t panel_gc9a01_reset(esp_lcd_panel_t *panel)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9a01->io;
    if (gc9a01->reset_gpio_num >= 0) {
        gpio_set_level(gc9a01->reset_gpio_num, gc9a01->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(gc9a01->reset_gpio_num, !gc9a01->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    } else {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), GC9A01_TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return ESP_OK;
}

static const gc9a01_lcd_init_cmd_t vendor_specific_init_default[] = {
    {0xfe, (uint8_t []){0x00}, 0, 0}, {0xef, (uint8_t []){0x00}, 0, 0},
    {0xeb, (uint8_t []){0x14}, 1, 0}, {0x84, (uint8_t []){0x60}, 1, 0},
    {0x85, (uint8_t []){0xff}, 1, 0}, {0x86, (uint8_t []){0xff}, 1, 0},
    {0x87, (uint8_t []){0xff}, 1, 0}, {0x8e, (uint8_t []){0xff}, 1, 0},
    {0x8f, (uint8_t []){0xff}, 1, 0}, {0x88, (uint8_t []){0x0a}, 1, 0},
    {0x89, (uint8_t []){0x23}, 1, 0}, {0x8a, (uint8_t []){0x00}, 1, 0},
    {0x8b, (uint8_t []){0x80}, 1, 0}, {0x8c, (uint8_t []){0x01}, 1, 0},
    {0x8d, (uint8_t []){0x03}, 1, 0}, {0x90, (uint8_t []){0x08, 0x08, 0x08, 0x08}, 4, 0},
    {0xff, (uint8_t []){0x60, 0x01, 0x04}, 3, 0}, {0xC3, (uint8_t []){0x13}, 1, 0},
    {0xC4, (uint8_t []){0x13}, 1, 0}, {0xC9, (uint8_t []){0x30}, 1, 0},
    {0xbe, (uint8_t []){0x11}, 1, 0}, {0xe1, (uint8_t []){0x10, 0x0e}, 2, 0},
    {0xdf, (uint8_t []){0x21, 0x0c, 0x02}, 3, 0},
    {0xF0, (uint8_t []){0x45, 0x09, 0x08, 0x08, 0x26, 0x2a}, 6, 0},
    {0xF1, (uint8_t []){0x43, 0x70, 0x72, 0x36, 0x37, 0x6f}, 6, 0},
    {0xF2, (uint8_t []){0x45, 0x09, 0x08, 0x08, 0x26, 0x2a}, 6, 0},
    {0xF3, (uint8_t []){0x43, 0x70, 0x72, 0x36, 0x37, 0x6f}, 6, 0},
    {0xed, (uint8_t []){0x1b, 0x0b}, 2, 0}, {0xae, (uint8_t []){0x77}, 1, 0},
    {0xcd, (uint8_t []){0x63}, 1, 0},
    {0x70, (uint8_t []){0x07, 0x07, 0x04, 0x0e, 0x0f, 0x09, 0x07, 0x08, 0x03}, 9, 0},
    {0xE8, (uint8_t []){0x34}, 1, 0},
    {0x60, (uint8_t []){0x38, 0x0b, 0x6D, 0x6D, 0x39, 0xf0, 0x6D, 0x6D}, 8, 0},
    {0x61, (uint8_t []){0x38, 0xf4, 0x6D, 0x6D, 0x38, 0xf7, 0x6D, 0x6D}, 8, 0},
    {0x62, (uint8_t []){0x38, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x38, 0x0F, 0x71, 0xEF, 0x70, 0x70}, 12, 0},
    {0x63, (uint8_t []){0x38, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x38, 0x13, 0x71, 0xF3, 0x70, 0x70}, 12, 0},
    {0x64, (uint8_t []){0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}, 7, 0},
    {0x66, (uint8_t []){0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}, 10, 0},
    {0x67, (uint8_t []){0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}, 10, 0},
    {0x74, (uint8_t []){0x10, 0x45, 0x80, 0x00, 0x00, 0x4E, 0x00}, 7, 0},
    {0x98, (uint8_t []){0x3e, 0x07}, 2, 0}, {0x99, (uint8_t []){0x3e, 0x07}, 2, 0},
};

static esp_err_t panel_gc9a01_init(esp_lcd_panel_t *panel)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9a01->io;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0), GC9A01_TAG, "send command failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) { gc9a01->madctl_val }, 1), GC9A01_TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]) { gc9a01->colmod_val }, 1), GC9A01_TAG, "send command failed");
    const gc9a01_lcd_init_cmd_t *init_cmds = gc9a01->init_cmds ? gc9a01->init_cmds : vendor_specific_init_default;
    uint16_t init_cmds_size = gc9a01->init_cmds ? gc9a01->init_cmds_size : sizeof(vendor_specific_init_default) / sizeof(gc9a01_lcd_init_cmd_t);
    for (int i = 0; i < init_cmds_size; i++) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), GC9A01_TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }
    ESP_LOGD(GC9A01_TAG, "send init commands success");
    return ESP_OK;
}

static esp_err_t panel_gc9a01_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9a01->io;
    x_start += gc9a01->x_gap; x_end += gc9a01->x_gap;
    y_start += gc9a01->y_gap; y_end += gc9a01->y_gap;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]) { (x_start >> 8) & 0xFF, x_start & 0xFF, ((x_end - 1) >> 8) & 0xFF, (x_end - 1) & 0xFF }, 4), GC9A01_TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]) { (y_start >> 8) & 0xFF, y_start & 0xFF, ((y_end - 1) >> 8) & 0xFF, (y_end - 1) & 0xFF }, 4), GC9A01_TAG, "send command failed");
    size_t len = (x_end - x_start) * (y_end - y_start) * gc9a01->fb_bits_per_pixel / 8;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len), GC9A01_TAG, "send color failed");
    return ESP_OK;
}

static esp_err_t panel_gc9a01_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9a01->io;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, invert_color_data ? LCD_CMD_INVON : LCD_CMD_INVOFF, NULL, 0), GC9A01_TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_gc9a01_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    if (mirror_x) gc9a01->madctl_val |= LCD_CMD_MX_BIT; else gc9a01->madctl_val &= ~LCD_CMD_MX_BIT;
    if (mirror_y) gc9a01->madctl_val |= LCD_CMD_MY_BIT; else gc9a01->madctl_val &= ~LCD_CMD_MY_BIT;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(gc9a01->io, LCD_CMD_MADCTL, (uint8_t[]) { gc9a01->madctl_val }, 1), GC9A01_TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_gc9a01_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    if (swap_axes) gc9a01->madctl_val |= LCD_CMD_MV_BIT; else gc9a01->madctl_val &= ~LCD_CMD_MV_BIT;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(gc9a01->io, LCD_CMD_MADCTL, (uint8_t[]) { gc9a01->madctl_val }, 1), GC9A01_TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_gc9a01_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    gc9a01->x_gap = x_gap; gc9a01->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_gc9a01_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    gc9a01_panel_t *gc9a01 = __containerof(panel, gc9a01_panel_t, base);
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(gc9a01->io, on_off ? LCD_CMD_DISPON : LCD_CMD_DISPOFF, NULL, 0), GC9A01_TAG, "send command failed");
    return ESP_OK;
}

// =================================================================================================
// SECTION 3: PUBLIC BSP DRIVER (IMPLEMENTATION)
// =================================================================================================

#define BSP_LCD_TAG "BSP_LCD" // 为BSP层日志定义TAG

// --- Module-level static variables ---
#define LCD_HOST            SPI3_HOST
#define PIN_NUM_LCD_CS      (-1)
#define PIN_NUM_LCD_PCLK    (GPIO_NUM_8)
#define PIN_NUM_LCD_DATA0   (GPIO_NUM_9)
#define PIN_NUM_LCD_RST     (GPIO_NUM_10)
#define PIN_NUM_LCD_DC      (GPIO_NUM_11)

#define LCD_H_RES           (240)
#define LCD_V_RES           (240)
#define LCD_BIT_PER_PIXEL   (16)

static esp_lcd_panel_io_handle_t s_io_handle = NULL;
static esp_lcd_panel_handle_t s_panel_handle = NULL;
static SemaphoreHandle_t s_dma_done_sem = NULL;
static SemaphoreHandle_t s_lcd_mutex = NULL;
static uint8_t s_last_brightness = 255;

// --- Private BSP functions ---

static bool IRAM_ATTR lcd_trans_done_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    SemaphoreHandle_t dma_done_sem = (SemaphoreHandle_t)user_ctx;
    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR(dma_done_sem, &task_woken);
    return (task_woken == pdTRUE);
}

static esp_err_t _bsp_lcd_set_brightness_internal(uint8_t brightness)
{
    if (brightness > 0) {
        s_last_brightness = brightness;
    }
    return esp_lcd_panel_io_tx_param(s_io_handle, 0xc9, (uint8_t[]){brightness}, 1);
}

// --- Public API Implementation (declared in bsp_lcd.h) ---

esp_err_t bsp_lcd_init(void)
{
    s_lcd_mutex = xSemaphoreCreateMutex();
    if (!s_lcd_mutex) {
        ESP_LOGE(BSP_LCD_TAG, "Failed to create LCD mutex!");
        return ESP_FAIL;
    }

    s_dma_done_sem = xSemaphoreCreateBinary();
    if (!s_dma_done_sem) {
        ESP_LOGE(BSP_LCD_TAG, "Failed to create DMA semaphore!");
        vSemaphoreDelete(s_lcd_mutex);
        return ESP_FAIL;
    }

    ESP_LOGI(BSP_LCD_TAG, "Initializing SPI bus...");
    const spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_LCD_DATA0,
        .miso_io_num = -1,
        .sclk_io_num = PIN_NUM_LCD_PCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (LCD_H_RES * LCD_V_RES * LCD_BIT_PER_PIXEL / 8),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(BSP_LCD_TAG, "Creating SPI panel IO handle...");
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

    ESP_LOGI(BSP_LCD_TAG, "Creating GC9A01 panel driver...");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
    };
    // 调用内部的GC9A01驱动创建函数
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(s_io_handle, &panel_config, &s_panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));
    bsp_lcd_set_brightness(255);

    ESP_LOGI(BSP_LCD_TAG, "LCD initialization complete.");
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

esp_err_t bsp_lcd_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    esp_err_t ret = ESP_FAIL;
    if (xSemaphoreTake(s_lcd_mutex, portMAX_DELAY) == pdTRUE) {
        ret = esp_lcd_panel_draw_bitmap(s_panel_handle, x_start, y_start, x_end, y_end, color_data);
        xSemaphoreGive(s_lcd_mutex);
    }
    return ret;
}

void bsp_lcd_wait_for_draw_done(void)
{
    xSemaphoreTake(s_dma_done_sem, portMAX_DELAY);
}

esp_err_t bsp_lcd_set_power(bool on)
{
    esp_err_t ret = ESP_FAIL;
    if (xSemaphoreTake(s_lcd_mutex, portMAX_DELAY) == pdTRUE) {
        if (on) {
            ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));
            ret = _bsp_lcd_set_brightness_internal(s_last_brightness);
            vTaskDelay(pdMS_TO_TICKS(20));
        } else {
            ESP_ERROR_CHECK(_bsp_lcd_set_brightness_internal(0));
            ret = esp_lcd_panel_disp_on_off(s_panel_handle, false);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        xSemaphoreGive(s_lcd_mutex);
    }
    return ret;
}

esp_err_t bsp_lcd_set_brightness(uint8_t brightness)
{
    esp_err_t ret = ESP_FAIL;
    if (xSemaphoreTake(s_lcd_mutex, portMAX_DELAY) == pdTRUE) {
        ret = _bsp_lcd_set_brightness_internal(brightness);
        xSemaphoreGive(s_lcd_mutex);
    }
    return ret;
}