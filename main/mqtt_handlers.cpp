#include "mqtt_handlers.h"
#include "http_handlers.h" // Для функции load_mqtt_settings
#include "esp_log.h"
#include <string>

static const char *TAG = "MQTT_HANDLER";

// Физическое определение глобального клиента
esp_mqtt_client_handle_t global_mqtt_client = NULL;

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Подключено к MQTT брокеру");
            esp_mqtt_client_subscribe(event->client, MQTT_TOPIC_SUB, 0);
            esp_mqtt_client_subscribe(event->client, "ESP_controller_1/valve/control", 0);
            global_mqtt_client = event->client;
            break;
        case MQTT_EVENT_DATA: {
            ESP_LOGI(TAG, "RAW TOPIC: '%.*s', RAW MSG: '%.*s'", event->topic_len, event->topic, event->data_len, event->data);
            std::string topic(event->topic, event->topic_len);
            std::string msg(event->data, event->data_len);
            
            if (topic == "ESP_controller_1/valve/control") {
                ESP_LOGI(TAG, "Команда управления краном: %s", msg.c_str());
                if (msg == "on") {
                   
                    valve_state = 1;
                    if (global_mqtt_client) {
                        esp_mqtt_client_publish(global_mqtt_client, "ESP_controller_1/valve/status", "on", 0, 1, 0);
                    }
                } else if (msg == "off") {
                  
                    valve_state = 0;
                    if (global_mqtt_client) {
                        esp_mqtt_client_publish(global_mqtt_client, "ESP_controller_1/valve/status", "off", 0, 1, 0);
                    }
                }
            }
            break;
        }
        default:
            break;
    }
}

// Новая функция инициализации MQTT (перенесена из main.cpp)
void mqtt_init() {
    char mqtt_server[64] = {0};
    char mqtt_user[32] = {0};
    char mqtt_pass[64] = {0};
    
    if (load_mqtt_settings(mqtt_server, sizeof(mqtt_server), mqtt_user, sizeof(mqtt_user), mqtt_pass, sizeof(mqtt_pass))) {
        esp_mqtt_client_config_t mqtt_cfg = {};
        char uri[80];
        snprintf(uri, sizeof(uri), "mqtt://%s", mqtt_server);
        mqtt_cfg.broker.address.uri = uri;
        mqtt_cfg.broker.address.port = MQTT_PORT;
        mqtt_cfg.credentials.username = mqtt_user;
        mqtt_cfg.credentials.authentication.password = mqtt_pass;
        
        esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
        esp_mqtt_client_start(client);
    } else {
        ESP_LOGW(TAG, "MQTT настройки не найдены в памяти, клиент не запущен");
    }
}