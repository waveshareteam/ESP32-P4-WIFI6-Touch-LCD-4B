#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "hello_world";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-P4 repository skeleton is running on ESP-IDF %s",
             esp_get_idf_version());
}
