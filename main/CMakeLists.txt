idf_component_register(
    SRCS 
        "main.cpp"
        "http_handlers.cpp"
        "spiffs_manager.cpp"
        "wifi_manager.cpp"
        "mqtt_handlers.cpp"
        "gpio_tasks.cpp"
        "oled.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi vfs fatfs nvs_flash spiffs esp_http_server esp_event driver log freertos esp_netif app_update esp_driver_ledc ssd1306
)

# Автоматическое создание образа SPIFFS из папки data
# Аргументы:
# 1. Имя раздела из partitions.csv (например, "storage")
# 2. Путь к папке с файлами (относительно этого CMakeLists.txt)
# 3. FLASH_IN_PROJECT - чтобы прошивать автоматически при команде idf.py flash

spiffs_create_partition_image(spiffs ../data FLASH_IN_PROJECT)