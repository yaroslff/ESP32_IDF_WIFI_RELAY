// Подключение стандартных и ESP-IDF библиотек
#include <stdio.h>
#include <string.h>
#include <string>
#include "esp_wifi.h"           // Работа с Wi-Fi
#include "esp_event.h"          // Система событий ESP
#include "esp_log.h"            // Логирование
#include "nvs_flash.h"          // NVS (энергонезависимая память)
#include "esp_vfs.h"            // Виртуальная файловая система
#include "esp_vfs_fat.h"        // FAT файловая система
#include "esp_spiffs.h"         // SPIFFS файловая система
#include "esp_http_server.h"    // Веб-сервер
#include "cJSON.h"              // Работа с JSON
#include "esp_netif.h"          // Сетевые интерфейсы ESP
#include "wifi_manager.h"       // Модуль управления Wi-Fi
#include "spiffs_manager.h"     // Модуль работы с SPIFFS
#include "http_handlers.h"      // Модуль HTTP-обработчиков
#include "mqtt_handlers.h"      // Модуль MQTT (содержит mqtt_init)

int relay_state_map[4] = {0}; // Глобальная переменная для хранения состояния реле

void gpio_init();

void defaultTassk(void *pvParameter);
void relayTask(void *pvParameter);



// --- Управление краном ---
extern int valve_state;
int valve_state = 0;

// Главная точка входа приложения ESP-IDF
extern "C" void app_main() {

    gpio_init();

    xTaskCreate(&defaultTassk, "defaultTask", 2048, NULL, 1, NULL);
    xTaskCreate(&relayTask, "relayTask", 2048, NULL, 1, NULL);

    // Инициализация энергонезависимой памяти (NVS)
    ESP_ERROR_CHECK(nvs_flash_init());
    // Монтирование файловой системы SPIFFS
    init_spiffs();

    // Буферы для хранения SSID и пароля Wi-Fi
    char ssid[32] = {0};
    char password[64] = {0};

    // Инициализация сетевого стека и событий
    esp_netif_init();
    esp_event_loop_create_default();
    // Создание сетевого интерфейса для клиента Wi-Fi (STA)
    sta_netif = esp_netif_create_default_wifi_sta();

    // Регистрация обработчиков событий Wi-Fi и IP
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);

    // Попытка загрузить сохранённые Wi-Fi настройки
    if (load_wifi_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
        // Если настройки найдены — подключаемся к Wi-Fi как клиент
        wifi_config_t wifi_config = {};
        strcpy((char *)wifi_config.sta.ssid, ssid);
        strcpy((char *)wifi_config.sta.password, password);

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_wifi_start();
        esp_wifi_connect();
        // Запуск веб-сервера
        server = start_webserver();
    } else {
        // Если настроек нет — запускаем точку доступа (AP)
        wifi_init_softap();
        // Запуск веб-сервера
        server = start_webserver();
    }

    // Инициализация MQTT (весь код убран в mqtt_handlers.cpp)
    mqtt_init();

    // После запуска веб-сервера:
    if (server) {
        ESP_LOGI("HTTP", "Веб-сервер запущен успешно");
    } else {
        ESP_LOGE("HTTP", "Ошибка запуска веб-сервера"); 
    }
}

void gpio_init(){
    gpio_config_t io_conf;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << 8) | (1ULL << 1) | (1ULL << 2) | (1ULL << 3) | (1ULL << 4) ;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
}

void relayTask(void *pvParameter) {
    while (1) {
        if(relay_state_map[0] == 1) {
            gpio_set_level(GPIO_NUM_1, 0);
        } else {
            gpio_set_level(GPIO_NUM_1, 1);
        }
        if(relay_state_map[1] == 1) {
            gpio_set_level(GPIO_NUM_2, 0);
        } else {
            gpio_set_level(GPIO_NUM_2, 1);
        }
        if(relay_state_map[2] == 1) {
            gpio_set_level(GPIO_NUM_3, 0);
        } else {
            gpio_set_level(GPIO_NUM_3, 1);
        }
        if(relay_state_map[3] == 1) {
            gpio_set_level(GPIO_NUM_4, 0);
        } else {
            gpio_set_level(GPIO_NUM_4, 1);
        }
       vTaskDelay(250); 
    }
}

void defaultTassk(void *pvParameter){
    while (1) {
        if(valve_state == 1) {
            gpio_set_level(GPIO_NUM_8, 1);
            vTaskDelay(100);
            gpio_set_level(GPIO_NUM_8, 0);
            vTaskDelay(100); 
        } else {
            gpio_set_level(GPIO_NUM_8, 1);
            vTaskDelay(500);
            gpio_set_level(GPIO_NUM_8, 0);
            vTaskDelay(500);
        }
    }
}

// --- Управление краном ---
void water_on() {
    ESP_LOGI("WATER", "Включить кран (water_on)");
}

void water_off() {
    ESP_LOGI("WATER", "Выключить кран (water_off)");
}