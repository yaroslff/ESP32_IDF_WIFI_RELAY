#pragma once
#include "esp_http_server.h"
#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

// Получить MIME-тип по расширению файла
const char* get_content_type(const char *filename);
// HTTP-обработчик для отдачи файлов
esp_err_t file_server_handler(httpd_req_t *req);
// HTTP-обработчик для сканирования Wi-Fi сетей
esp_err_t scan_handler(httpd_req_t *req);
// HTTP-обработчик для сохранения Wi-Fi настроек
esp_err_t save_handler(httpd_req_t *req);
// HTTP-обработчик для сохранения MQTT настроек
esp_err_t mqtt_save_handler(httpd_req_t *req);
// Загрузить MQTT настройки из NVS
bool load_mqtt_settings(char* server, size_t server_len, char* user, size_t user_len, char* pass, size_t pass_len);

esp_err_t ota_update_handler(httpd_req_t *req);
// Запуск веб-сервера
httpd_handle_t start_webserver();

// Глобальная переменная состояния крана
extern int valve_state;

// --- Обработчики управления краном ---
esp_err_t valve_on_handler(httpd_req_t *req);
esp_err_t valve_off_handler(httpd_req_t *req);
esp_err_t valve_status_handler(httpd_req_t *req);
// --- Обработчики управления реле ---
esp_err_t relay_on_handler(httpd_req_t *req);
esp_err_t relay_off_handler(httpd_req_t *req);
esp_err_t relay_status_handler(httpd_req_t *req);

extern esp_mqtt_client_handle_t global_mqtt_client;

#ifdef __cplusplus
}
#endif
