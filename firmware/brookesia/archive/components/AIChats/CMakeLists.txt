set(PROJ_SRC ${CMAKE_CURRENT_LIST_DIR})

file(GLOB_RECURSE PROJ_SRCS_C ${PROJ_SRC}/*.c)
file(GLOB_RECURSE PROJ_SRCS_CPP ${PROJ_SRC}/*.cpp)
file(GLOB_RECURSE PROJ_SRCS_CC ${PROJ_SRC}/*.cc)

idf_component_register(
    SRCS ${PROJ_SRCS_C} ${PROJ_SRCS_CPP} ${PROJ_SRCS_CC}
    INCLUDE_DIRS ${PROJ_SRC}
    REQUIRES brookesia_core bsp_extra HardwareApps esp_audio_codec esp_audio_effects esp-sr xiaozhi-fonts esp_event esp_netif esp_websocket_client nvs_flash
    PRIV_REQUIRES
        app_update
        esp32_p4_wifi6_touch_lcd_4b
        esp_http_client
        cjson
        efuse
        esp_hw_support
        esp_app_format
        esp_partition
        esp_timer
        mbedtls
        spi_flash
)

set_source_files_properties(
    ${PROJ_SRCS_CPP} ${PROJ_SRCS_CC}
    PROPERTIES
        COMPILE_FLAGS "-Wno-missing-field-initializers"
)