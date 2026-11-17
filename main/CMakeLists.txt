idf_component_register(
    SRCS "main.cpp" "camera_ov2640.c" "image_source.cpp" "quedas.c" "wifi.c" "web.c" "teste.c" "sirene.c"
    INCLUDE_DIRS "."
    REQUIRES spiffs esp_timer esp32-camera pedestrian_detect
             esp_wifi esp_netif esp_event nvs_flash esp_http_server esp_driver_gpio
)
# grava a pasta imagens/ na particao "storage" junto com o firmware
spiffs_create_partition_image(storage ../imagens FLASH_IN_PROJECT)
