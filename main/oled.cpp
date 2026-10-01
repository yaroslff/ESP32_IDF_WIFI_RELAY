#include "oled.h"

SSD1306_t dev;

void oled_init(){
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    dev._flip = false; // Установка ориентации дисплея (false - нормальная, true - перевернутая)
    ssd1306_init(&dev, 128, 64);

    ssd1306_clear_screen(&dev, false);
    ssd1306_contrast(&dev, 0xff);

    ssd1306_display_text(&dev, 0, "ESP32", 5, false);
}