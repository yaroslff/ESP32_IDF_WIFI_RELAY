#pragma once

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
//#include "arduino.h"     