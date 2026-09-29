#pragma once
#include "esp_wifi.h"
#include "esp_netif.h"
#include <stdbool.h>
#include "esp_http_server.h" // Для типа httpd_handle_t

// Инициализация точки доступа (AP)
void wifi_init_softap();
// Сохранение Wi-Fi настроек в NVS
void save_wifi_credentials(const char *ssid, const char *password);
// Загрузка Wi-Fi настроек из NVS
bool load_wifi_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len);
// Глобальный обработчик событий Wi-Fi и IP
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
// Глобальные переменные для сетевых интерфейсов и статусов
extern esp_netif_t *sta_netif;   // STA-интерфейс (клиент)
extern esp_netif_t *ap_netif;    // AP-интерфейс (точка доступа)
extern bool ap_created;          // Флаг: создана ли точка доступа
extern const char *TAG;          // Тег для логирования
extern httpd_handle_t server;    // Хэндл веб-сервера
