#include "feature_anim_player.h"
#include "bsp_lcd.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"

#define TAG "ANIM_PLAYER_JPEG"

// --- 动画与显示配置 ---
#define FRAME_WIDTH          240
#define FRAME_HEIGHT         240
#define BYTES_PER_PIXEL      2 // 对于RGB565格式，每个像素占2字节
#define FRAME_DELAY_MS       50 // 帧延迟，可根据实际效果调整
#define FRAME_BUFFER_SIZE    (FRAME_WIDTH * FRAME_HEIGHT * BYTES_PER_PIXEL)

// --- 【重要修改点 1】: 创建一个静态缓冲区来容纳JPEG文件 ---
// !!! 请根据您项目中最大的JPEG文件大小来调整此值 !!!
// 例如, 如果最大的JPEG文件是28KB, 您可以设置为 30 * 1024
#define MAX_JPEG_FILE_SIZE   (40 * 1024)
static uint8_t g_jpeg_file_buffer[MAX_JPEG_FILE_SIZE];


// --- 动画信息结构体与数据库 ---
typedef struct {
    const char* base_name; // JPEG文件的基本名, 例如 "aini"
    int frame_count;       // 该动画的总帧数
} anim_info_t;

// !!重要!!: 请根据您实际的图片文件, 在这里更新每个动画的 `frame_count`
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


// --- 模块私有 (static) 变量 ---
static volatile uint8_t g_target_brightness = 0xFF;
static volatile bool g_target_on_off_state = true;
static volatile bool g_current_on_off_state = true;

// static uint16_t *frame_buffer1 = NULL;
// static uint16_t *frame_buffer2 = NULL;
static uint16_t *g_frame_buffer = NULL; // 用一个更通用的名字

static SemaphoreHandle_t dma_done_sem = NULL;

static SemaphoreHandle_t g_lcd_mutex = NULL;
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

    // --- 【修改关键点】 ---
    // 1. 检查文件大小是否超过静态缓冲区容量
    if (file_size > MAX_JPEG_FILE_SIZE) {
        ESP_LOGE(TAG, "JPEG文件太大! 文件大小: %ld bytes, 缓冲区大小: %d bytes", file_size, MAX_JPEG_FILE_SIZE);
        fclose(f);
        return ESP_FAIL;
    }

    // 2. 读取文件内容到静态全局缓冲区
    if (fread(g_jpeg_file_buffer, 1, file_size, f) != file_size) {
        ESP_LOGE(TAG, "读取JPEG文件失败: %s", jpeg_path);
        fclose(f);
        return ESP_FAIL;
    }
    fclose(f); // 文件已读入内存,可以关闭文件了

    // --- 后续解码流程不变 ---
    jpeg_dec_handle_t jpeg_dec = NULL;
    jpeg_dec_io_t jpeg_io;
    jpeg_dec_header_info_t out_info;

    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE;
    
    if (jpeg_dec_open(&config, &jpeg_dec) != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "jpeg_dec_open failed");
        return ESP_FAIL;
    }
    
    memset(&jpeg_io, 0, sizeof(jpeg_dec_io_t));
    jpeg_io.inbuf = g_jpeg_file_buffer; // 3. 使用静态全局缓冲区
    jpeg_io.inbuf_len = file_size;
    jpeg_io.outbuf = (uint8_t*)target_buffer;

    if (jpeg_dec_parse_header(jpeg_dec, &jpeg_io, &out_info) != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "jpeg_dec_parse_header failed for %s", jpeg_path);
        jpeg_dec_close(jpeg_dec); // 确保在出错时关闭解码器
        return ESP_FAIL;
    }

    jpeg_dec_process(jpeg_dec, &jpeg_io);
    jpeg_dec_close(jpeg_dec);

    // 4. 不再需要 cleanup 和 free
    return ESP_OK;
}


static void jpeg_animation_task(void *pvParameters)
{
    ESP_LOGI(TAG, "JPEG动画播放任务已启动 (单缓冲模式)");

    // --- 预解码第一帧 ---
    const anim_info_t* initial_anim = &g_anim_database[ANIM_TYPE_AINI];
    char jpeg_path[64];
    snprintf(jpeg_path, sizeof(jpeg_path), "/storage/%s/%s_0.jpg", initial_anim->base_name, initial_anim->base_name);
    
    ESP_LOGI(TAG, "预解码第一帧: %s", jpeg_path);
    if (decode_jpeg_to_buffer(jpeg_path, g_frame_buffer) != ESP_OK) { // 解码到唯一的缓冲区
        ESP_LOGE(TAG, "预解码失败，屏幕可能显示为黑色");
    }
    while (1) {

        
        // --- 屏幕状态控制 (这部分可以保留在循环开始) ---
        if (g_current_on_off_state != g_target_on_off_state) {
            xSemaphoreTake(g_lcd_mutex, portMAX_DELAY);
            esp_lcd_panel_disp_on_off(g_bsp_panel_handle, g_target_on_off_state);
            g_current_on_off_state = g_target_on_off_state;
            xSemaphoreGive(g_lcd_mutex);
        }

        // --- 获取当前动画状态 ---
        taskENTER_CRITICAL(&animation_spinlock);
        const anim_info_t* active_anim = g_current_anim_info;
        int frame_index = g_current_frame_index;
        taskEXIT_CRITICAL(&animation_spinlock);

        bool should_draw = (g_current_on_off_state && active_anim != NULL && active_anim->frame_count > 0);

        if (should_draw) {
            // 单缓冲模式: 解码 -> 发送 -> 等待 -> 更新帧号
            
            // 1. 解码当前帧到缓冲区
            snprintf(jpeg_path, sizeof(jpeg_path), "/storage/%s/%s_%d.jpg", active_anim->base_name, active_anim->base_name, frame_index);
            if (decode_jpeg_to_buffer(jpeg_path, g_frame_buffer) == ESP_OK) {
                // 2. 将解码好的缓冲区通过DMA发送到屏幕
                xSemaphoreTake(g_lcd_mutex, portMAX_DELAY);
                esp_lcd_panel_draw_bitmap(g_bsp_panel_handle, 0, 0, FRAME_WIDTH, FRAME_HEIGHT, g_frame_buffer);
                xSemaphoreGive(g_lcd_mutex);

                // 3. 等待DMA传输完成 (这一步是性能瓶颈)
                xSemaphoreTake(dma_done_sem, portMAX_DELAY);
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

// --- 公共API实现 (无需修改) ---

esp_err_t anim_player_init(void)
{
    ESP_LOGI(TAG, "初始化动画播放器...");
    dma_done_sem = xSemaphoreCreateBinary();
    g_lcd_mutex = xSemaphoreCreateMutex();
    if (dma_done_sem == NULL || g_lcd_mutex == NULL) {
        ESP_LOGE(TAG, "创建同步对象失败!");
        return ESP_FAIL;
    }
    // 初始状态下信号量是可用的，bsp_lcd_init中的回调函数会在DMA完成后再次give
    // xSemaphoreGive(dma_done_sem); // 在bsp_lcd_init之前调用是正确的

    return bsp_lcd_init(dma_done_sem);
}

void anim_player_task_start(void)
{
    // --- 在这里加入诊断代码 ---
    ESP_LOGI(TAG, "================ MEMORY DIAGNOSTICS ================");
    ESP_LOGI(TAG, "Total free heap size: %d bytes", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    ESP_LOGI(TAG, "Total free DMA-capable heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_DMA));
    ESP_LOGI(TAG, "Largest free contiguous DMA block: %d bytes", heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    ESP_LOGI(TAG, "Required single buffer size: %d bytes", FRAME_BUFFER_SIZE);
    ESP_LOGI(TAG, "====================================================");
    // --- 诊断代码结束 ---
    
    ESP_LOGI(TAG, "为DMA单缓冲分配内存...");
    g_frame_buffer = (uint16_t *)heap_caps_malloc(FRAME_BUFFER_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

    if (!g_frame_buffer) {
         ESP_LOGW(TAG, "内部DMA内存分配失败! 尝试从外部PSRAM分配...");
         g_frame_buffer = (uint16_t *)heap_caps_malloc(FRAME_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
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
    
    // 加锁以确保动画信息和帧索引的切换是原子操作
    xSemaphoreTake(g_lcd_mutex, portMAX_DELAY);
    taskENTER_CRITICAL(&animation_spinlock);
    
    g_current_anim_info = &g_anim_database[anim_type];
    g_current_frame_index = 0; // 每次切换动画都从第0帧开始
    
    taskEXIT_CRITICAL(&animation_spinlock);
    xSemaphoreGive(g_lcd_mutex);

    ESP_LOGI(TAG, "切换动画到 '%s', 共 %d 帧", g_anim_database[anim_type].base_name, g_anim_database[anim_type].frame_count);
}

esp_err_t anim_player_display_off(void) {
    g_target_on_off_state = false;
    ESP_LOGI(TAG, "请求关闭屏幕");
    return ESP_OK;
}

esp_err_t anim_player_display_on(void) {
    g_target_on_off_state = true;
    ESP_LOGI(TAG, "请求开启屏幕");
    return ESP_OK;
}

esp_err_t anim_player_set_brightness(uint8_t brightness) {
    g_target_brightness = brightness;
    // 注意: 设置亮度的硬件控制逻辑需要您在bsp_lcd或此处实现
    ESP_LOGI(TAG, "请求设置亮度为: 0x%02X", brightness);
    return ESP_OK;
}