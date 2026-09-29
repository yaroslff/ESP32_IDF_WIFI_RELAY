#include "spiffs_manager.h"
#include "esp_spiffs.h"
#include "esp_log.h"

extern const char *TAG;

// Инициализация и монтирование файловой системы SPIFFS
void init_spiffs() {
    static bool initialized = false; // Флаг, чтобы не инициализировать повторно
    if (initialized) return;
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",           // Точка монтирования
        .partition_label = "spiffs",      // Имя раздела
        .max_files = 5,                    // Максимум открытых файлов
        .format_if_mount_failed = true     // Форматировать при ошибке монтирования
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS");
    }
    initialized = true;
}
