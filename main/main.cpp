#include "main.h"
#include "gpio_tasks.h"
int relay_state_map[4] = {0}; // Глобальная переменная для хранения состояния реле



void defaultTassk(void *pvParameter);




// Главная точка входа приложения ESP-IDF
extern "C" void app_main() {

   
    xTaskCreate(&defaultTassk, "defaultTask", 2048, NULL, 1, NULL);
    gpio_tasks_init(); // Инициализация задач GPIO (включая реле и светодиод)

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




void defaultTassk(void *pvParameter){
    while (1) {
       
            gpio_set_level(GPIO_NUM_8, 1);
            vTaskDelay(250);
            gpio_set_level(GPIO_NUM_8, 0);
            vTaskDelay(500);
      
    }
}
