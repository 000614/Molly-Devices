#include "feature_anim_player.h"
#include "bsp_lcd.h" // <-- 关键改变：现在只依赖BSP的头文件

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"

#define TAG "ANIM_PLAYER"

// --- 动画配置 ---
#define FRAME_DELAY_MS       50 // 帧延迟，可根据实际效果调整
// !! 移除屏幕尺寸相关的宏定义，将从BSP获取 !!

// --- JPEG文件缓冲区 ---
#define MAX_JPEG_FILE_SIZE   (40 * 1024)
static uint8_t g_jpeg_file_buffer[MAX_JPEG_FILE_SIZE];

// --- 动画信息结构体与数据库 ---
typedef struct {
    const char* base_name;
    int frame_count;
} anim_info_t;

static const anim_info_t g_anim_database[ANIM_TYPE_MAX] = {
    [ANIM_TYPE_AINI]         = {"aini", 25},
    [ANIM_TYPE_DAIJI]        = {"daiji", 100},
    [ANIM_TYPE_KU]           = {"ku", 50},
    [ANIM_TYPE_SHUIJIAO]     = {"shuijiao", 120},
    [ANIM_TYPE_ZHAYAN]       = {"zhayan", 20},
    [ANIM_TYPE_ZUOGUOYOUPAN] = {"zuoguyoupan", 70},
    [ANIM_TYPE_SHENGYIN_0]   = {"shengyin_0", 1},
    [ANIM_TYPE_SHENGYIN_10]  = {"shengyin_10", 1},
    [ANIM_TYPE_SHENGYIN_20]  = {"shengyin_20", 1},
    [ANIM_TYPE_SHENGYIN_30]  = {"shengyin_30", 1},
    [ANIM_TYPE_SHENGYIN_40]  = {"shengyin_40", 1},
    [ANIM_TYPE_SHENGYIN_50]  = {"shengyin_50", 1},
    [ANIM_TYPE_SHENGYIN_60]  = {"shengyin_60", 1},
    [ANIM_TYPE_SHENGYIN_70]  = {"shengyin_70", 1},
    [ANIM_TYPE_SHENGYIN_80]  = {"shengyin_80", 1},
    [ANIM_TYPE_SHENGYIN_90]  = {"shengyin_90", 1},
    [ANIM_TYPE_SHENGYIN_100] = {"shengyin_100", 1},
};

// --- 模块私有变量 ---
static volatile bool g_target_on_off_state = true;
static volatile bool g_current_on_off_state = true;

static uint16_t *g_frame_buffer = NULL;

static SemaphoreHandle_t g_player_mutex = NULL; // 用于保护动画状态等共享资源
static portMUX_TYPE animation_spinlock = portMUX_INITIALIZER_UNLOCKED;

static const anim_info_t* g_current_anim_info = NULL;
static volatile int g_current_frame_index = 0;

static esp_err_t decode_jpeg_to_buffer(const char* jpeg_path, uint16_t* target_buffer)
{
    FILE *f = fopen(jpeg_path, "r");
    if (!f) {
        ESP_LOGW(TAG, "打开文件失败: %s", jpeg_path);
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size > MAX_JPEG_FILE_SIZE) {
        ESP_LOGE(TAG, "JPEG文件太大! 文件大小: %ld bytes, 缓冲区大小: %d bytes", file_size, MAX_JPEG_FILE_SIZE);
        fclose(f);
        return ESP_FAIL;
    }

    if (fread(g_jpeg_file_buffer, 1, file_size, f) != file_size) {
        ESP_LOGE(TAG, "读取JPEG文件失败: %s", jpeg_path);
        fclose(f);
        return ESP_FAIL;
    }
    fclose(f);

    jpeg_dec_handle_t jpeg_dec = NULL;
    jpeg_dec_io_t jpeg_io = {0};
    jpeg_dec_header_info_t out_info;
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE;
    
    if (jpeg_dec_open(&config, &jpeg_dec) != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "jpeg_dec_open failed");
        return ESP_FAIL;
    }
    
    jpeg_io.inbuf = g_jpeg_file_buffer;
    jpeg_io.inbuf_len = file_size;
    jpeg_io.outbuf = (uint8_t*)target_buffer;

    if (jpeg_dec_parse_header(jpeg_dec, &jpeg_io, &out_info) != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "jpeg_dec_parse_header failed for %s", jpeg_path);
        jpeg_dec_close(jpeg_dec);
        return ESP_FAIL;
    }

    jpeg_dec_process(jpeg_dec, &jpeg_io);
    jpeg_dec_close(jpeg_dec);
    return ESP_OK;
}

static void jpeg_animation_task(void *pvParameters)
{
    ESP_LOGI(TAG, "JPEG动画播放任务已启动");
    char jpeg_path[64];

    while (1) {
        // --- 屏幕电源状态控制 ---
        if (g_current_on_off_state != g_target_on_off_state) {
            bsp_lcd_set_power(g_target_on_off_state);
            g_current_on_off_state = g_target_on_off_state;
        }

        // --- 获取当前动画状态 ---
        taskENTER_CRITICAL(&animation_spinlock);
        const anim_info_t* active_anim = g_current_anim_info;
        int frame_index = g_current_frame_index;
        taskEXIT_CRITICAL(&animation_spinlock);

        bool should_draw = (g_current_on_off_state && active_anim != NULL && active_anim->frame_count > 0);

        if (should_draw) {
            // 1. 解码当前帧到缓冲区
            snprintf(jpeg_path, sizeof(jpeg_path), "/storage/%s/%s_%d.jpg", active_anim->base_name, active_anim->base_name, frame_index);
            if (decode_jpeg_to_buffer(jpeg_path, g_frame_buffer) == ESP_OK) {
                // 2. 将缓冲区通过BSP接口发送到屏幕
                bsp_lcd_draw_bitmap(0, 0, bsp_lcd_get_width(), bsp_lcd_get_height(), g_frame_buffer);
                
                // 3. 等待BSP通知DMA传输完成
                bsp_lcd_wait_for_draw_done();
            }

            // 4. 更新到下一帧
            taskENTER_CRITICAL(&animation_spinlock);
            g_current_frame_index = (g_current_frame_index + 1) % active_anim->frame_count;
            taskEXIT_CRITICAL(&animation_spinlock);
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(FRAME_DELAY_MS));
    }
}

// --- 公共API实现 ---

esp_err_t anim_player_init(void)
{
    ESP_LOGI(TAG, "初始化动画播放器...");
    g_player_mutex = xSemaphoreCreateMutex();
    if (g_player_mutex == NULL) {
        ESP_LOGE(TAG, "创建播放器互斥锁失败!");
        return ESP_FAIL;
    }
    // 初始化LCD硬件
    return bsp_lcd_init();
}

void anim_player_task_start(void)
{
    const uint16_t frame_width = bsp_lcd_get_width();
    const uint16_t frame_height = bsp_lcd_get_height();
    const size_t frame_buffer_size = frame_width * frame_height * 2; // 假设为RGB565

    ESP_LOGI(TAG, "================ MEMORY DIAGNOSTICS ================");
    ESP_LOGI(TAG, "Screen Resolution: %dx%d", frame_width, frame_height);
    ESP_LOGI(TAG, "Required single buffer size: %d bytes", frame_buffer_size);
    ESP_LOGI(TAG, "Total free heap size: %d bytes", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    ESP_LOGI(TAG, "Total free DMA-capable heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_DMA));
    ESP_LOGI(TAG, "====================================================");
    
    ESP_LOGI(TAG, "为DMA单缓冲分配内存...");
    g_frame_buffer = (uint16_t *)heap_caps_malloc(frame_buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

    if (!g_frame_buffer) {
         ESP_LOGW(TAG, "内部DMA内存分配失败! 尝试从外部PSRAM分配...");
         g_frame_buffer = (uint16_t *)heap_caps_malloc(frame_buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    }

    if (!g_frame_buffer) {
        ESP_LOGE(TAG, "DMA单缓冲内存分配彻底失败!");
        return;
    }

    anim_player_switch_animation(ANIM_TYPE_AINI);

    xTaskCreatePinnedToCore(
        jpeg_animation_task, "jpeg_anim_task", 4096, NULL, 10, NULL, 1
    );
}

void anim_player_switch_animation(anim_type_t anim_type) {
    if (anim_type >= ANIM_TYPE_MAX) {
        ESP_LOGW(TAG, "未知的动画类型: %d", anim_type);
        return;
    }
    
    xSemaphoreTake(g_player_mutex, portMAX_DELAY);
    taskENTER_CRITICAL(&animation_spinlock);
    
    g_current_anim_info = &g_anim_database[anim_type];
    g_current_frame_index = 0;
    
    taskEXIT_CRITICAL(&animation_spinlock);
    xSemaphoreGive(g_player_mutex);

    ESP_LOGI(TAG, "切换动画到 '%s', 共 %d 帧", g_anim_database[anim_type].base_name, g_anim_database[anim_type].frame_count);
}

esp_err_t anim_player_display_off(void) {
    // g_target_on_off_state = false;
    bsp_lcd_set_power(0);
    ESP_LOGI(TAG, "请求关闭屏幕");
    return ESP_OK;
}

esp_err_t anim_player_display_on(void) {
    // g_target_on_off_state = true;
    bsp_lcd_set_power(1);
    ESP_LOGI(TAG, "请求开启屏幕");
    return ESP_OK;
}

esp_err_t anim_player_set_brightness(uint8_t brightness) {
    ESP_LOGI(TAG, "请求设置亮度为: %d", brightness);
    return bsp_lcd_set_brightness(brightness);
}