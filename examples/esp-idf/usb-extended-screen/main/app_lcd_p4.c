/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include "esp_check.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/jpeg_decode.h"
#include "app_lcd.h"
#include "bsp/display.h"
#include "sdkconfig.h"

static const char *TAG = "app_lcd";

static esp_lcd_panel_handle_t display_handle;
static jpeg_decoder_handle_t jpgd_handle = NULL;

static jpeg_decode_cfg_t decode_cfg = {
    .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
    .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
};

static uint32_t out_size = 0;
static void *lcd_buffer[EXAMPLE_LCD_BUF_NUM];
static uint8_t buf_index = 0;
static bool init_ok = false;

void app_lcd_draw(uint8_t *buf, uint32_t len, uint16_t width, uint16_t height)
{
    if (!init_ok || !buf || len == 0) {
        return;
    }
    if (width != EXAMPLE_LCD_H_RES || height != EXAMPLE_LCD_V_RES) {
        ESP_LOGW(TAG, "drop decoded frame with unexpected size: %ux%u (expected %ux%u)",
                 width, height, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
        return;
    }
    static int fps_count = 0;
    static int64_t start_time = 0;
    fps_count++;
    if (fps_count == 50) {
        int64_t end_time = esp_timer_get_time();
        ESP_LOGI(TAG, "fps: %f", 1000000.0 / ((end_time - start_time) / 50.0));
        start_time = end_time;
        fps_count = 0;
    }
    esp_err_t ret = jpeg_decoder_process(jpgd_handle, &decode_cfg, buf, len, lcd_buffer[buf_index], EXAMPLE_LCD_BUF_LEN, &out_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "JPEG decode failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_lcd_panel_draw_bitmap(display_handle, 0, 0, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, lcd_buffer[buf_index]);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LCD draw failed: %s", esp_err_to_name(ret));
        return;
    }

    buf_index = (buf_index + 1) == EXAMPLE_LCD_BUF_NUM ? 0 : (buf_index + 1);
}


esp_err_t app_lcd_init(void)
{
    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .intr_priority = 1,
        .timeout_ms = 50,
    };

    ESP_RETURN_ON_ERROR(jpeg_new_decoder_engine(&decode_eng_cfg, &jpgd_handle), TAG, "create JPEG decoder failed");

    esp_lcd_panel_io_handle_t mipi_dbi_io = NULL;

    ESP_LOGI(TAG, "Initialize MIPI DSI bus p4");

    ESP_RETURN_ON_ERROR(bsp_display_new(NULL, &display_handle, &mipi_dbi_io), TAG, "initialize display failed");
    ESP_RETURN_ON_ERROR(bsp_display_backlight_on(), TAG, "enable display backlight failed");

#if EXAMPLE_LCD_BUF_NUM == 1
    ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_get_frame_buffer(display_handle, 1, &lcd_buffer[0]), TAG, "get LCD frame buffer failed");
#elif EXAMPLE_LCD_BUF_NUM == 2
    ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_get_frame_buffer(display_handle, 2, &lcd_buffer[0], &lcd_buffer[1]), TAG, "get LCD frame buffers failed");
#else
    ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_get_frame_buffer(display_handle, 3, &lcd_buffer[0], &lcd_buffer[1], &lcd_buffer[2]), TAG, "get LCD frame buffers failed");
#endif

    init_ok = true;
    return ESP_OK;
}
