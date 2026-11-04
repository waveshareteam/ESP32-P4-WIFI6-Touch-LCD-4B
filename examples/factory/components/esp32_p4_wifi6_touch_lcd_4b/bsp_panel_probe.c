/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "bsp_panel_probe.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/mipi_dsi_ll.h"

static const char *TAG = "lcd_probe";
#define PROBE_TIMEOUT_US (100000)

static esp_err_t send_header(dsi_host_dev_t *host, mipi_dsi_data_type_t type,
                             uint8_t high, uint8_t low, int64_t deadline)
{
    while (mipi_dsi_host_ll_gen_is_cmd_fifo_full(host)) {
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }
    mipi_dsi_host_ll_gen_set_packet_header(host, 0, type, high, low);
    return ESP_OK;
}

/* IDF 5.5 DBI reads wait forever if the panel does not return a payload.
 * Follow its HAL packet sequence with a deadline instead. A timed-out probe
 * must be followed by destroying and recreating the DSI bus before any write.
 * No private DSI handle layout is used: this board has one bus and VC zero.
 */
static esp_err_t bounded_rx_param(esp_lcd_panel_io_t *io, int command,
                                  void *param, size_t size)
{
    if (!io || !param || !size || size > UINT16_MAX || command < 0 || command > UINT8_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    dsi_host_dev_t *host = MIPI_DSI_LL_GET_HOST(0);
    const int64_t deadline = esp_timer_get_time() + PROBE_TIMEOUT_US;
    memset(param, 0, size);
    while (!mipi_dsi_host_ll_gen_is_read_fifo_empty(host)) {
        (void)mipi_dsi_host_ll_gen_read_payload_fifo(host);
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }
    }
    esp_err_t ret = send_header(host, MIPI_DSI_DT_SET_MAXIMUM_RETURN_PKT,
                                (size >> 8) & 0xff, size & 0xff, deadline);
    if (ret != ESP_OK) {
        return ret;
    }
    mipi_dsi_host_ll_enable_video_mode(host, false);
    mipi_dsi_host_ll_enable_bta(host, true);
    mipi_dsi_host_ll_gen_set_rx_vcid(host, 0);
    ret = send_header(host, MIPI_DSI_DT_DCS_READ_0, 0, command, deadline);
    if (ret != ESP_OK) {
        goto done;
    }
    while (mipi_dsi_host_ll_gen_is_read_cmd_busy(host) ||
           mipi_dsi_host_ll_gen_is_read_fifo_empty(host)) {
        if (esp_timer_get_time() >= deadline) {
            ret = ESP_ERR_TIMEOUT;
            goto done;
        }
        vTaskDelay(1);
    }
    size_t copied = 0;
    while (!mipi_dsi_host_ll_gen_is_read_fifo_empty(host)) {
        uint32_t word = mipi_dsi_host_ll_gen_read_payload_fifo(host);
        for (int i = 0; i < 4 && copied < size; ++i) {
            ((uint8_t *)param)[copied++] = (word >> (i * 8)) & 0xff;
        }
        if (esp_timer_get_time() >= deadline) {
            ret = ESP_ERR_TIMEOUT;
            goto done;
        }
    }
    ret = copied == size ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
done:
    mipi_dsi_host_ll_enable_bta(host, false);
    return ret;
}

void bsp_panel_io_enable_bounded_reads(esp_lcd_panel_io_handle_t io)
{
    io->rx_param = bounded_rx_param;
}

static esp_err_t probe_write(uint8_t command, uint8_t value)
{
    return send_header(MIPI_DSI_LL_GET_HOST(0), MIPI_DSI_DT_DCS_SHORT_WRITE_1,
                       value, command, esp_timer_get_time() + PROBE_TIMEOUT_US);
}

esp_err_t bsp_panel_probe(esp_lcd_panel_io_handle_t io, bsp_panel_kind_t *kind)
{
    *kind = BSP_PANEL_UNKNOWN;
    uint8_t id[3] = {0};
    esp_err_t ret = esp_lcd_panel_io_rx_param(io, 0x04, id, sizeof(id));
    ESP_LOGI(TAG, "DCS display ID: %02X %02X %02X (%s)", id[0], id[1], id[2], esp_err_to_name(ret));
    if (ret != ESP_OK) {
        return ret;
    }
    /* ST7703 datasheet reset values; these module ID bytes can be changed by OTP.
     * Never infer a chip ID from its product name or merely a nonzero response. */
    if (id[0] == 0x38 && id[1] == 0x21 && id[2] == 0x1f) {
        *kind = BSP_PANEL_ST7703;
        return ESP_OK;
    }
    /* JD9365DA user guide, GET_ID_VERSION, page 1 registers 8D/8E:
     * fixed production number 93/65. The standard 04/DA/DB/DC are module OTP IDs.
     * These writes only unlock/select the command page; no OTP command is sent.
     * The caller always resets the panel again before its vendor initialization. */
    const uint8_t unlock[][2] = {{0xe0, 0x00}, {0xe1, 0x93}, {0xe2, 0x65}, {0xe3, 0xf8}, {0xe0, 0x01}};
    for (size_t i = 0; i < sizeof(unlock) / sizeof(unlock[0]); ++i) {
        ret = probe_write(unlock[i][0], unlock[i][1]);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    uint8_t chip_id[2] = {0};
    ret = esp_lcd_panel_io_rx_param(io, 0x8d, &chip_id[0], 1);
    if (ret == ESP_OK) {
        ret = esp_lcd_panel_io_rx_param(io, 0x8e, &chip_id[1], 1);
    }
    ESP_LOGI(TAG, "JD9365 page 1 chip ID: %02X %02X (%s)", chip_id[0], chip_id[1], esp_err_to_name(ret));
    if (ret == ESP_OK && chip_id[0] == 0x93 && chip_id[1] == 0x65) {
        *kind = BSP_PANEL_JD9365;
    }
    return ret;
}
