#include "http_handlers.h"
#include "wifi_manager.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mqtt_client.h"
#include "esp_ota_ops.h"
#include "esp_system.h"

extern esp_mqtt_client_handle_t global_mqtt_client;

// Глобальные переменные для хранения последнего управляемого реле
static int last_relay_num = 0;
static int last_relay_state = 0;
extern int relay_state_map[4]; // Глобальная переменная для хранения состояния реле

// Определение MIME-типа по расширению файла
const char* get_content_type(const char *filename) {
    if (strstr(filename, ".html")) return "text/html";
    if (strstr(filename, ".css")) return "text/css";
    if (strstr(filename, ".js")) return "application/javascript";
    if (strstr(filename, ".png")) return "image/png";
    if (strstr(filename, ".jpg")) return "image/jpeg";
    if (strstr(filename, ".ico")) return "image/x-icon";
    return "application/octet-stream";
}

// HTTP-обработчик для отдачи файлов из SPIFFS
esp_err_t file_server_handler(httpd_req_t *req) {
    const char *uri = req->uri;
    if (strcmp(uri, "/") == 0) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/index.html");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    if (strstr(uri, "..") != NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_OK;
    }
    char file_path[520];
    snprintf(file_path, sizeof(file_path), "/spiffs%s", uri);
    FILE *file = fopen(file_path, "r");
    if (!file) {
        ESP_LOGE(TAG, "File not found: %s", file_path);
        httpd_resp_send_404(req);
        return ESP_OK;
    }
    httpd_resp_set_type(req, get_content_type(file_path));
    char buffer[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, read_bytes);
    }
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// HTTP-обработчик для сканирования Wi-Fi сетей
esp_err_t scan_handler(httpd_req_t *req) {
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {.min = 100, .max = 300},
            .passive = 0
        },
        .home_chan_dwell_time = 0,
        .channel_bitmap = {
            .ghz_2_channels = 0,
            .ghz_5_channels = 0
        },
        .coex_background_scan = false
    };

    esp_wifi_scan_start(&scan_config, true);
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        httpd_resp_send(req, "[]", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    wifi_ap_record_t ap_info[ap_count];
    esp_wifi_scan_get_ap_records(&ap_count, ap_info);
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < ap_count; i++) {
        cJSON_AddItemToArray(root, cJSON_CreateString((char *)ap_info[i].ssid));
    }
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// HTTP-обработчик для сохранения Wi-Fi настроек
esp_err_t save_handler(httpd_req_t *req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    const cJSON *ssid_json = cJSON_GetObjectItem(json, "ssid");
    const cJSON *password_json = cJSON_GetObjectItem(json, "password");
    if (!cJSON_IsString(ssid_json) || !cJSON_IsString(password_json)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON format");
        return ESP_FAIL;
    }
    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.sta.ssid, ssid_json->valuestring);
    strcpy((char *)wifi_config.sta.password, password_json->valuestring);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_connect();
    save_wifi_credentials(ssid_json->valuestring, password_json->valuestring);
    httpd_resp_send(req, "Настройки сохранены! Подключение...", HTTPD_RESP_USE_STRLEN);
    cJSON_Delete(json);
    return ESP_OK;
}

#define NVS_MQTT_NAMESPACE "mqttcfg"
#define NVS_MQTT_KEY_SERVER "server"
#define NVS_MQTT_KEY_USER   "user"
#define NVS_MQTT_KEY_PASS   "pass"

static esp_err_t save_mqtt_settings(const char* server, const char* user, const char* pass) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_MQTT_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    nvs_set_str(nvs, NVS_MQTT_KEY_SERVER, server);
    nvs_set_str(nvs, NVS_MQTT_KEY_USER, user);
    nvs_set_str(nvs, NVS_MQTT_KEY_PASS, pass);
    err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

bool load_mqtt_settings(char* server, size_t server_len, char* user, size_t user_len, char* pass, size_t pass_len) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_MQTT_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) return false;
    esp_err_t ok1 = nvs_get_str(nvs, NVS_MQTT_KEY_SERVER, server, &server_len);
    esp_err_t ok2 = nvs_get_str(nvs, NVS_MQTT_KEY_USER, user, &user_len);
    esp_err_t ok3 = nvs_get_str(nvs, NVS_MQTT_KEY_PASS, pass, &pass_len);
    nvs_close(nvs);
    return ok1 == ESP_OK && ok2 == ESP_OK && ok3 == ESP_OK;
}

esp_err_t mqtt_save_handler(httpd_req_t *req) {
    char buf[200];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return ESP_OK;
    }
    const cJSON *server = cJSON_GetObjectItem(json, "server");
    const cJSON *user = cJSON_GetObjectItem(json, "username");
    const cJSON *pass = cJSON_GetObjectItem(json, "password");
    if (!cJSON_IsString(server) || !cJSON_IsString(user) || !cJSON_IsString(pass)) {
        cJSON_Delete(json);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"Invalid fields\"}");
        return ESP_OK;
    }
    esp_err_t err = save_mqtt_settings(server->valuestring, user->valuestring, pass->valuestring);
    cJSON_Delete(json);
    httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK) {
        httpd_resp_sendstr(req, "{\"success\":true}");
    } else {
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"NVS error\"}");
    }
    return ESP_OK;
}




// Универсальный обработчик управления реле через POST
esp_err_t relay_control_handler(httpd_req_t *req) {
    char buf[200];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        ESP_LOGE(TAG, "Failed to receive POST data");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Парсим JSON из тела запроса
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Извлекаем номер реле
    const cJSON *relay_json = cJSON_GetObjectItem(json, "relay");
    if (!cJSON_IsNumber(relay_json)) {
        ESP_LOGE(TAG, "Invalid relay parameter");
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid relay parameter");
        return ESP_FAIL;
    }
    
    // Извлекаем состояние
    const cJSON *state_json = cJSON_GetObjectItem(json, "state");
    if (!cJSON_IsString(state_json)) {
        ESP_LOGE(TAG, "Invalid state parameter");
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid state parameter");
        return ESP_FAIL;
    }
    
    int relay_num = relay_json->valueint;
    const char *state_str = state_json->valuestring;
    
    // Валидация номера реле
    if (relay_num < 1 || relay_num > 4) {
        ESP_LOGE(TAG, "Invalid relay number: %d", relay_num);
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Relay number must be 1-4");
        return ESP_FAIL;
    }
    
    // Определяем состояние (1 = on, 0 = off)
    int relay_state = 0;
    if (strcmp(state_str, "on") == 0) {
        relay_state = 1;
    } else if (strcmp(state_str, "off") == 0) {
        relay_state = 0;
    } else {
        ESP_LOGE(TAG, "Invalid state value: %s", state_str);
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "State must be 'on' or 'off'");
        return ESP_FAIL;
    }
    
    // Сохраняем в глобальные переменные
    last_relay_num = relay_num;
    last_relay_state = relay_state;
    relay_state_map[relay_num - 1] = relay_state;
    
    // Логируем действие
    ESP_LOGI(TAG, "Relay control command: Relay %d -> %s", relay_num, state_str);
    
    cJSON_Delete(json);
    
    // Отправляем успешный ответ
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}



// Обработчик загрузки OTA прошивки
esp_err_t ota_update_handler(httpd_req_t *req) {
    esp_err_t err;
    esp_ota_handle_t update_handle = 0;
    
    // 1. Находим неактивный раздел (ota_0 или ota_1)
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Не найден раздел для OTA");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Partition not found");
        return ESP_FAIL;
    }

    // 2. Начинаем процесс OTA
    ESP_LOGI(TAG, "Начало OTA. Размер файла: %d байт", req->content_len);
    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка esp_ota_begin: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Begin failed");
        return ESP_FAIL;
    }

    // 3. Читаем данные из HTTP-запроса чанками и пишем во флэш
    char buf[1024]; // Размер чанка
    int received = 0;
    int remaining = req->content_len;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue; // Таймаут чтения, пробуем еще раз
            }
            ESP_LOGE(TAG, "Ошибка чтения данных (получено %d из %d)", received, req->content_len);
            esp_ota_abort(update_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            return ESP_FAIL;
        }

        err = esp_ota_write(update_handle, buf, recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Ошибка записи во флэш: %s", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Write failed");
            return ESP_FAIL;
        }

        remaining -= recv_len;
        received += recv_len;
    }

    // 4. Завершаем OTA и проверяем целостность (подпись/CRC)
    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка esp_ota_end: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA End failed");
        return ESP_FAIL;
    }

    // 5. Переключаем загрузочный раздел
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка esp_ota_set_boot_partition: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Boot partition setup failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA успешно завершена. Перезагрузка...");
    httpd_resp_sendstr(req, "OK");

    // Запускаем асинхронную перезагрузку, чтобы сервер успел отправить ответ "OK" браузеру
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ESP_OK;
}

// Запуск веб-сервера и регистрация всех обработчиков
httpd_handle_t start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 15;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server");
        return NULL;
    }
    
    // Регистрируем обработчик сканирования Wi-Fi
    httpd_uri_t uri_scan = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = scan_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_scan);
    
    // Регистрируем обработчик сохранения Wi-Fi настроек
    httpd_uri_t uri_save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_save);
    
    // Регистрируем обработчик сохранения MQTT настроек
    httpd_uri_t uri_mqtt_save = {
        .uri = "/mqtt-save",
        .method = HTTP_POST,
        .handler = mqtt_save_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_mqtt_save);
    
   
    
    
    
    
    
    // Регистрируем обработчик управления реле (POST)
    httpd_uri_t uri_relay_control = {
        .uri = "/relay/control",
        .method = HTTP_POST,
        .handler = relay_control_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_relay_control);

    httpd_uri_t uri_ota_update = {
    .uri       = "/update",
    .method    = HTTP_POST,
    .handler   = ota_update_handler,
    .user_ctx  = NULL

    };
   
    httpd_register_uri_handler(server, &uri_ota_update);
    

    
    // Регистрируем универсальный обработчик файлов
    httpd_uri_t uri_catch_all = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = file_server_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_catch_all);

    
    
    return server;
}