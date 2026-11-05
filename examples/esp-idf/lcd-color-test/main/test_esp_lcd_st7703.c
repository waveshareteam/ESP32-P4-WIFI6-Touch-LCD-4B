/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"

#if SOC_MIPI_DSI_SUPPORTED

#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_board_manager.h"
#include "dev_display_lcd.h"
#include "periph_ledc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"
#include "esp_lcd_mipi_dsi.h"

#define TEST_LCD_H_RES (720)
#define TEST_LCD_V_RES (720)

#define TEST_DELAY_TIME_MS (3000)

static char *TAG = "lcd_color_test";
static esp_lcd_panel_handle_t panel_handle = NULL;
static dev_display_lcd_handles_t *lcd_handles = NULL;
static periph_ledc_handle_t *brightness_handle = NULL;
static uint32_t lcd_bits_per_pixel;
static SemaphoreHandle_t refresh_finish = NULL;

IRAM_ATTR static bool test_notify_refresh_ready(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    SemaphoreHandle_t refresh_finish = (SemaphoreHandle_t)user_ctx;
    BaseType_t need_yield = pdFALSE;

    xSemaphoreGiveFromISR(refresh_finish, &need_yield);

    return (need_yield == pdTRUE);
}

static void test_init_lcd(void)
{
    dev_display_lcd_config_t *lcd_config = NULL;

    ESP_LOGI(TAG, "Initialize Board Manager LCD device");
    TEST_ESP_OK(esp_board_manager_init_device_by_name("display_lcd"));
    TEST_ESP_OK(esp_board_manager_get_device_handle("display_lcd", (void **)&lcd_handles));
    TEST_ESP_OK(esp_board_manager_get_device_config("display_lcd", (void **)&lcd_config));
    TEST_ASSERT_NOT_NULL(lcd_handles);
    TEST_ASSERT_NOT_NULL(lcd_handles->panel_handle);
    TEST_ASSERT_NOT_NULL(lcd_config);
    TEST_ASSERT_TRUE(lcd_config->bits_per_pixel > 0);
    panel_handle = lcd_handles->panel_handle;
    lcd_bits_per_pixel = lcd_config->bits_per_pixel;

    ESP_LOGI(TAG, "Initialize Board Manager LCD backlight device");
    TEST_ESP_OK(esp_board_manager_init_device_by_name("lcd_brightness"));
    TEST_ESP_OK(esp_board_manager_get_device_handle("lcd_brightness", (void **)&brightness_handle));
    TEST_ASSERT_NOT_NULL(brightness_handle);
    TEST_ESP_OK(ledc_set_duty(brightness_handle->speed_mode, brightness_handle->channel, 1023));
    TEST_ESP_OK(ledc_update_duty(brightness_handle->speed_mode, brightness_handle->channel));
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));

    refresh_finish = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(refresh_finish);
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = test_notify_refresh_ready,
    };
    TEST_ESP_OK(esp_lcd_dpi_panel_register_event_callbacks(panel_handle, &cbs, refresh_finish));
}

static void test_deinit_lcd(void)
{
    TEST_ESP_OK(esp_board_manager_deinit_device_by_name("display_lcd"));
    panel_handle = NULL;
    lcd_handles = NULL;
    lcd_bits_per_pixel = 0;

    TEST_ESP_OK(esp_board_manager_deinit_device_by_name("lcd_brightness"));
    brightness_handle = NULL;

    vSemaphoreDelete(refresh_finish);
    refresh_finish = NULL;
}

static void test_draw_color_bar(esp_lcd_panel_handle_t panel_handle, uint16_t h_res, uint16_t v_res)
{
    uint8_t byte_per_pixel = (lcd_bits_per_pixel + 7) / 8;
    uint16_t row_line = v_res / byte_per_pixel / 8;
    uint8_t *color = (uint8_t *)heap_caps_calloc(1, row_line * h_res * byte_per_pixel, MALLOC_CAP_DMA);

    for (int j = 0; j < byte_per_pixel * 8; j++)
    {
        for (int i = 0; i < row_line * h_res; i++)
        {
            for (int k = 0; k < byte_per_pixel; k++)
            {
                color[i * byte_per_pixel + k] = (BIT(j) >> (k * 8)) & 0xff;
            }
        }
        TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, j * row_line, h_res, (j + 1) * row_line, color));
        xSemaphoreTake(refresh_finish, portMAX_DELAY);
    }

    uint16_t color_line = row_line * byte_per_pixel * 8;
    uint16_t res_line = v_res - color_line;
    if (res_line)
    {
        for (int i = 0; i < res_line * h_res; i++)
        {
            for (int k = 0; k < byte_per_pixel; k++)
            {
                color[i * byte_per_pixel + k] = 0xff;
            }
        }
        TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, color_line, h_res, v_res, color));
        xSemaphoreTake(refresh_finish, portMAX_DELAY);
    }

    free(color);
}

TEST_CASE("test LCD to draw pattern with MIPI interface", "[lcd][draw_pattern]")
{
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_lcd();

    ESP_LOGI(TAG, "Show color bar pattern drawn by hardware");
    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_VERTICAL));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));
    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_HORIZONTAL));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));
    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_NONE));

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_lcd();
}

TEST_CASE("test LCD to draw color bar with MIPI interface", "[lcd][draw_color_bar]")
{
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_lcd();

    ESP_LOGI(TAG, "Show color bar drawn by software");
    test_draw_color_bar(panel_handle, TEST_LCD_H_RES, TEST_LCD_V_RES);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_lcd();
}

TEST_CASE("test LCD to rotate with MIPI interface", "[lcd][rotate]")
{
    esp_err_t ret = ESP_OK;

    uint16_t w = 0;
    uint16_t h = 0;
    int64_t t = 0;

    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_lcd();

    ESP_LOGI(TAG, "Rotate the screen");
    for (size_t i = 0; i < 8; i++)
    {
        if (ret != ESP_ERR_NOT_SUPPORTED)
        {
            if (i & 4)
            {
                w = TEST_LCD_V_RES;
                h = TEST_LCD_H_RES;
            }
            else
            {
                w = TEST_LCD_H_RES;
                h = TEST_LCD_V_RES;
            }
        }

        TEST_ASSERT_NOT_EQUAL(esp_lcd_panel_mirror(panel_handle, i & 2, i & 1), ESP_FAIL);
        ret = esp_lcd_panel_swap_xy(panel_handle, i & 4);
        TEST_ASSERT_NOT_EQUAL(ret, ESP_FAIL);

        ESP_LOGI(TAG, "Rotation: %d", i);
        t = esp_timer_get_time();
        test_draw_color_bar(panel_handle, w, h);
        t = esp_timer_get_time() - t;
        ESP_LOGI(TAG, "@resolution %dx%d time per frame=%.2fMS\r\n", w, h, (float)t / 1000.0f);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_lcd();
}

// Some resources are lazy allocated in the LCD driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (300)

static size_t before_free_8bit;
static size_t before_free_32bit;

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    unity_utils_check_leak(before_free_8bit, after_free_8bit, "8BIT", TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_check_leak(before_free_32bit, after_free_32bit, "32BIT", TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void)
{
    /**
     *      _ ____   ___  _  __  ____
     *     | |  _ \ / _ \/ |/ /_| ___|
     *  _  | | | | | (_) | | '_ \___ \
     * | |_| | |_| |\__, | | (_) |__) |
     *  \___/|____/   /_/|_|\___/____/
     */
    printf("      _ ____   ___  _  __  ____  \r\n");
    printf("     | |  _ \\ / _ \\/ |/ /_| ___| \r\n");
    printf("  _  | | | | | (_) | | '_ \\___ \\ \r\n");
    printf(" | |_| | |_| |\\__, | | (_) |__) |\r\n");
    printf(" \\___/|____/   /_/|_|\\___/____/ \r\n");
    // unity_run_menu();
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_lcd();

//    ESP_LOGI(TAG, "Show color bar pattern drawn by hardware");
//    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_VERTICAL));
    ESP_LOGI(TAG, "Show color bar pattern drawn by hardware");
    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_VERTICAL));
}
#endif
