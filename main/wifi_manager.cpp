#include "wifi_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include <string.h>
#include "http_handlers.h" // Для start_webserver

// Глобальные переменные (см. заголовок)
const char *TAG = "wifi_setup";
bool ap_created = false;
esp_netif_t *sta_netif = NULL;
esp_netif_t *ap_netif = NULL;
httpd_handle_t server = NULL;

// Сохраняет Wi-Fi настройки (SSID и пароль) в энергонезависимую память (NVS)
void save_wifi_credentials(const char *ssid, const char *password) {
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("wifi_creds", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "password", password));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
}

// Загружает Wi-Fi настройки (SSID и пароль) из энергонезависимой памяти (NVS)
bool load_wifi_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len) {
    nvs_handle_t nvs_handle;
    if (nvs_open("wifi_creds", NVS_READONLY, &nvs_handle) != ESP_OK) return false;
    esp_err_t err = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_len);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return false;
    }
    err = nvs_get_str(nvs_handle, "password", password, &password_len);
    nvs_close(nvs_handle);
    return err == ESP_OK;
}

// Инициализация точки доступа (AP) с фиксированными параметрами
void wifi_init_softap() {
    if (ap_created) return; // Не создавать повторно
    ap_netif = esp_netif_create_default_wifi_ap();
    if (!ap_netif) {
        ESP_LOGE(TAG, "Failed to create default AP netif");
        return;
    }
    wifi_config_t ap_config = {};
    strcpy((char *)ap_config.ap.ssid, "ESP32_Setup"); // Имя сети
    ap_config.ap.ssid_len = strlen("ESP32_Setup");
    strcpy((char *)ap_config.ap.password, "12345678"); // Пароль
    ap_config.ap.max_connection = 4; // Максимум клиентов
    ap_config.ap.authmode = (strlen("12345678") > 0) ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
    // Настройка IP-адреса точки доступа
    esp_netif_ip_info_t ipInfo;
    ipInfo.ip.addr = ESP_IP4TOADDR(192, 168, 4, 1);
    ipInfo.gw.addr = ESP_IP4TOADDR(192, 168, 4, 1);
    ipInfo.netmask.addr = ESP_IP4TOADDR(255, 255, 255, 0);
    esp_netif_set_ip_info(ap_netif, &ipInfo);
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_dhcps_start(ap_netif);
    ap_created = true;
}

// Глобальный обработчик событий Wi-Fi и IP
// При отключении от Wi-Fi — запускает точку доступа и веб-сервер
// При получении IP — перезапускает веб-сервер
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (server) {
            httpd_stop(server);
            server = NULL;
        }
        wifi_init_softap();
        server = start_webserver();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        if (server) {
            httpd_stop(server);
            server = NULL;
        }
        server = start_webserver();
    }
}
