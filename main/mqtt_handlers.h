#pragma once
#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

// Макросы топиков
#define MQTT_PORT 1883
#define MQTT_TOPIC_PUB "esp32/test/pub"
#define MQTT_TOPIC_SUB "esp32/test/sub"

// Глобальные переменные
extern esp_mqtt_client_handle_t global_mqtt_client;
extern int valve_state;


// Функции инициализации и обработчика
void mqtt_init();
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

#ifdef __cplusplus
}
#endif