#include "oled.h"

SSD1306_t dev;

void oled_display_task(void *pvParameter);

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
        ssd1306_display_text(&dev, 1, "Hello, World!", 13, false);
        vTaskDelay(pdMS_TO_TICKS(2000)); // Задержка 2 секунды

        ssd1306_clear_screen(&dev, false);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Задержка 1 секунда
    }
}