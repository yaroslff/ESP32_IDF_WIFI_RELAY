#include "oled.h"

SSD1306_t dev;

void oled_display_task(void *pvParameter);
void update_display_network_info(SSD1306_t *dev);

void oled_init(){
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    dev._flip = false; // Установка ориентации дисплея (false - нормальная, true - перевернутая)
    ssd1306_init(&dev, 128, 64);

    ssd1306_clear_screen(&dev, false);
    ssd1306_contrast(&dev, 0xff);

   xTaskCreate(&oled_display_task, "oled_display_task", 2048, NULL, 1, NULL);
}

void oled_display_task(void *pvParameter) {
    while (1) {
        // Пример отображения текста на дисплее
        update_display_network_info(&dev);
        vTaskDelay(pdMS_TO_TICKS(2000)); // Задержка 2 секунды

        ssd1306_clear_screen(&dev, false);
       // vTaskDelay(pdMS_TO_TICKS(1000)); // Задержка 1 секунда
    }
}

// Функция принимает указатель на уже инициализированную структуру дисплея
void update_display_network_info(SSD1306_t *dev) {
    char ip_str[16] = "0.0.0.0";
    int8_t rssi = 0;
    bool is_sta = false;
    esp_netif_ip_info_t ip_info;

    // 1. Проверяем интерфейс клиента (STA - домашняя сеть)
    esp_netif_t *netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif_sta && esp_netif_get_ip_info(netif_sta, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
        is_sta = true;
    } else {
        // 2. Если STA не подключен, проверяем интерфейс точки доступа (AP)
        esp_netif_t *netif_ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        if (netif_ap && esp_netif_get_ip_info(netif_ap, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
        }
    }

    char temp_str[32];
    char display_ip[32];
    char display_rssi[32];

    // 3. Форматируем строки и добиваем их пробелами строго до 16 символов (ключ %-16s)
    snprintf(temp_str, sizeof(temp_str), "IP:%s", ip_str);
    snprintf(display_ip, sizeof(display_ip), "%-16s", temp_str);

    // Если подключены к роутеру, выводим RSSI. Иначе — статус режима AP.
    if (is_sta) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            rssi = ap_info.rssi;
        }
        snprintf(temp_str, sizeof(temp_str), "RSSI:%d dBm", rssi);
    } else {
        snprintf(temp_str, sizeof(temp_str), "Mode: AP");
    }
    snprintf(display_rssi, sizeof(display_rssi), "%-16s", temp_str);

    // 4. Вывод на экран строго 16 символов. 
    // Лишние хвосты от старых значений затираются пробелами аппаратно.
    ssd1306_display_text(dev, 0, display_ip, 16, false);
    ssd1306_display_text(dev, 1, display_rssi, 16, false);
}