#ifndef FEATURE_ANIM_PLAYER_H
#define FEATURE_ANIM_PLAYER_H

#include "esp_err.h"
#include <stdint.h>

// 明确定义所有动画类型的枚举
typedef enum {
    ANIM_TYPE_AINI,
    ANIM_TYPE_DAIJI,
    ANIM_TYPE_KU,
    ANIM_TYPE_SHUIJIAO,
    ANIM_TYPE_ZHAYAN,
    ANIM_TYPE_ZUOGUOYOUPAN,
    ANIM_TYPE_SHENGYIN_0,
    ANIM_TYPE_SHENGYIN_10,
    ANIM_TYPE_SHENGYIN_20,
    ANIM_TYPE_SHENGYIN_30,
    ANIM_TYPE_SHENGYIN_40,
    ANIM_TYPE_SHENGYIN_50,
    ANIM_TYPE_SHENGYIN_60,
    ANIM_TYPE_SHENGYIN_70,
    ANIM_TYPE_SHENGYIN_80,
    ANIM_TYPE_SHENGYIN_90,
    ANIM_TYPE_SHENGYIN_100,
    ANIM_TYPE_MAX // 用于计算枚举成员的数量
} anim_type_t;

// --- 公共 API ---

/**
 * @brief 初始化动画播放器及其依赖 (如LCD)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t anim_player_init(void);

/**
 * @brief 创建并启动动画播放任务
 */
void anim_player_task_start(void);

/**
 * @brief 切换到指定的动画
 * @param anim_type 要播放的动画的类型
 */
void anim_player_switch_animation(anim_type_t anim_type);

/**
 * @brief 请求关闭屏幕显示
 * @return esp_err_t
 */
esp_err_t anim_player_display_off(void);

/**
 * @brief 请求打开屏幕显示
 * @return esp_err_t
 */
esp_err_t anim_player_display_on(void);

/**
 * @brief 请求设置屏幕亮度
 * @param brightness 亮度值
 * @return esp_err_t
 */
esp_err_t anim_player_set_brightness(uint8_t brightness);

#endif // FEATURE_ANIM_PLAYER_H