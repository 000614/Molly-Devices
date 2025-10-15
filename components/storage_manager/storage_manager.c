#include "storage_manager.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include <dirent.h>

#define TAG "STORAGE"
#define MOUNT_PATH "/storage"

// static wl_handle_t s_wl_handle = WL_INVALID_HANDLE; // <-- 这行不再需要，可以删除或注释掉

esp_err_t storage_init(void)
{
    ESP_LOGI(TAG, "Initializing and mounting FATFS partition in Read-Only mode...");

    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false, // 必须为 false，以保护我们烧录的数据
        .max_files = 5,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };

    // 使用正确的、可用的只读挂载函数
    esp_err_t err = esp_vfs_fat_spiflash_mount_ro(
        MOUNT_PATH,
        "storage",
        &mount_config
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS partition (%s). Make sure 'storage' partition exists and is valid.", esp_err_to_name(err));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "FATFS partition mounted successfully at %s", MOUNT_PATH);

    // --- 调试代码：列出目录内容 ---
    ESP_LOGI(TAG, "Listing contents of %s:", MOUNT_PATH);
    DIR *d = opendir(MOUNT_PATH);
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            ESP_LOGI(TAG, "  - Found: %s", dir->d_name);
        }
        closedir(d);
    } else {
        ESP_LOGE(TAG, "Could not open directory %s", MOUNT_PATH);
    }
    return ESP_OK;
}