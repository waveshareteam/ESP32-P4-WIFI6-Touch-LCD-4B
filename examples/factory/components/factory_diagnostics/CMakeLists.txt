idf_component_register(
    SRCS "factory_diagnostics.cpp"
    INCLUDE_DIRS "include"
    REQUIRES brookesia_core MusicPlayer esp32_p4_wifi6_touch_lcd_4b esp_hosted esp_driver_uart esp_driver_i2c esp_psram spi_flash
)
