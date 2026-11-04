set(FACTORY_C6_BUNDLED_IMAGE "${CMAKE_CURRENT_LIST_DIR}/../../spiffs/c6/esp_hosted-3.0.10.bin")
if(NOT EXISTS "${FACTORY_C6_BUNDLED_IMAGE}")
    message(FATAL_ERROR "Required bundled C6 image is missing: ${FACTORY_C6_BUNDLED_IMAGE}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${FACTORY_C6_BUNDLED_IMAGE}")
file(SHA256 "${FACTORY_C6_BUNDLED_IMAGE}" FACTORY_C6_BUNDLED_SHA256)
configure_file("${CMAKE_CURRENT_LIST_DIR}/c6_bundled_hash.hpp.in" "${CMAKE_CURRENT_BINARY_DIR}/include/c6_bundled_hash.hpp" @ONLY)

idf_component_register(
    SRCS "c6_updater.cpp"
    INCLUDE_DIRS "include" "${CMAKE_CURRENT_BINARY_DIR}/include"
    REQUIRES esp_hosted esp_driver_uart esp_psram mbedtls bootloader_support esp_app_format
)
