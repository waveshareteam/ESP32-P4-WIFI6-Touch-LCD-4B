/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * JD9365DA MIPI-DSI panel driver. The structure follows esp_lcd_st7703 / esp_lcd_jd9365; the built-in
 * initialization list is the 8904ANT vendor sequence of the ESP32-P4-WIFI6-Touch-LCD-4B 4-inch
 * 720x720 2-lane panel.
 */

#pragma once

#include <stdint.h>
#include "soc/soc_caps.h"

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_idf_version.h"

#define ESP_LCD_JD9365_VER_MAJOR (1)
#define ESP_LCD_JD9365_VER_MINOR (0)
#define ESP_LCD_JD9365_VER_PATCH (0)

/* ESP-IDF v6.0 renamed the DPI panel pixel format field. */
#ifndef JD9365_LCD_DPI_CONFIG_COLOR_FORMAT
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#ifndef LCD_COLOR_PIXEL_FORMAT_RGB565
#define LCD_COLOR_PIXEL_FORMAT_RGB565 LCD_COLOR_FMT_RGB565
#endif
#ifndef LCD_COLOR_PIXEL_FORMAT_RGB666
#define LCD_COLOR_PIXEL_FORMAT_RGB666 LCD_COLOR_FMT_RGB888
#endif
#ifndef LCD_COLOR_PIXEL_FORMAT_RGB888
#define LCD_COLOR_PIXEL_FORMAT_RGB888 LCD_COLOR_FMT_RGB888
#endif
#define JD9365_LCD_DPI_CONFIG_COLOR_FORMAT(px_format) .in_color_format = px_format
#define JD9365_LCD_DPI_CONFIG_DMA2D
#else
#define JD9365_LCD_DPI_CONFIG_COLOR_FORMAT(px_format) .pixel_format = px_format
#define JD9365_LCD_DPI_CONFIG_DMA2D .flags.use_dma2d = true,
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief LCD panel initialization commands.
     *
     */
    typedef struct
    {
        int cmd;               /*<! The specific LCD command */
        const void *data;      /*<! Buffer that holds the command specific data */
        size_t data_bytes;     /*<! Size of `data` in memory, in bytes */
        unsigned int delay_ms; /*<! Delay in milliseconds after this command */
    } jd9365_lcd_init_cmd_t;

    /**
     * @brief LCD panel vendor configuration.
     *
     * @note  This structure needs to be passed to the `vendor_config` field in `esp_lcd_panel_dev_config_t`.
     *
     */
    typedef struct
    {
        const jd9365_lcd_init_cmd_t *init_cmds; /*!< Pointer to initialization commands array. Set to NULL if using default commands.
                                                 *   The array should be declared as `static const` and positioned outside the function.
                                                 *   Please refer to `vendor_specific_init_default` in source file.
                                                 */
        uint16_t init_cmds_size;                /*<! Number of commands in above array */
        struct
        {
            esp_lcd_dsi_bus_handle_t dsi_bus;             /*!< MIPI-DSI bus configuration */
            const esp_lcd_dpi_panel_config_t *dpi_config; /*!< MIPI-DPI panel configuration */
            uint8_t lane_num;                             /*!< Number of MIPI-DSI data lanes, programmed into panel register 0x80.
                                                           *   Leave 0 to keep the 2-lane default of the built-in sequence. */
        } mipi_config;
        struct
        {
            unsigned int use_mipi_interface : 1; /*<! Set to 1 if using MIPI interface, default is RGB interface */
            unsigned int mirror_by_cmd : 1;      /*<! The `mirror()` function will be implemented by LCD command if set to 1. */
        } flags;
    } jd9365_vendor_config_t;

    /**
     * @brief Create LCD panel for model JD9365DA
     *
     * @note  Vendor specific initialization can be different between manufacturers, should consult the LCD supplier for
     *        initialization sequence code. The built-in sequence targets the 4-inch 720x720 2-lane panel delivered with
     *        the ESP32-P4-WIFI6-Touch-LCD-4B (8904ANT register list, page 0 register 0x80 = 0x01 for two data lanes).
     *
     * @param[in]  io LCD panel IO handle
     * @param[in]  panel_dev_config General panel device configuration
     * @param[out] ret_panel Returned LCD panel handle
     * @return
     *      - ESP_ERR_INVALID_ARG   if parameter is invalid
     *      - ESP_OK                on success
     *      - Otherwise             on fail
     */
    esp_err_t esp_lcd_new_panel_jd9365(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                       esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief MIPI-DSI bus configuration structure
 *
 * @param[in] lane_num Number of data lanes
 * @param[in] lane_mbps Lane bit rate in Mbps
 *
 */
#define JD9365_PANEL_BUS_DSI_2CH_CONFIG() \
    {                                     \
        .bus_id = 0,                      \
        .num_data_lanes = 2,              \
        .phy_clk_src = 0,                 \
        .lane_bit_rate_mbps = 480,        \
    }

/**
 * @brief MIPI-DBI panel IO configuration structure
 *
 */
#define JD9365_PANEL_IO_DBI_CONFIG() \
    {                                \
        .virtual_channel = 0,        \
        .lcd_cmd_bits = 8,           \
        .lcd_param_bits = 8,         \
    }

/**
 * @brief MIPI DPI configuration structure for the 4-inch 720x720 2-lane panel
 *
 * The vendor timing is HFP 30, HBP 30, HSYNC 24, VFP 12, VBP 16, VSYNC 2 with a 36 MHz pixel clock, which
 * gives a 59.7 Hz refresh rate.
 *
 * @note  The ESP32-P4 derives the DPI clock from PLL_F240M (240 MHz) through an integer divider, so 36 MHz
 *        is not reachable. Requesting it makes the HAL shrink the line length to keep the expected refresh
 *        rate, and that compensation is applied to the front porch: 804 * (240 / 7) / 36 - 804 = -38,
 *        i.e. HFP becomes 30 - 38 = -8. The DSI bridge then emits no valid stream: the panel turns solid
 *        blue and `esp_lcd_panel_draw_bitmap()` never signals frame completion, which deadlocks any task
 *        that flushes through the LVGL adapter.
 *        Requesting the achievable clock instead (240 / 7 = 34.286 MHz) makes the compensation exactly
 *        zero, so the vendor blanking is programmed verbatim and the panel runs at about 56.9 Hz.
 *
 * @note  refresh_rate = (dpi_clock_freq_mhz * 1000000) / (h_res + hsync_pulse_width + hsync_back_porch + hsync_front_porch)
 *                                                      / (v_res + vsync_pulse_width + vsync_back_porch + vsync_front_porch)
 *
 * @param[in] px_format Pixel format of the panel
 *
 */
#define JD9365_720_720_PANEL_60HZ_DPI_CONFIG(px_format) \
    {                                                   \
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,    \
        .dpi_clock_freq_mhz = (240.0f / 7.0f),          \
        .virtual_channel = 0,                           \
        JD9365_LCD_DPI_CONFIG_COLOR_FORMAT(px_format),  \
        .num_fbs = 1,                                   \
        .video_timing = {                               \
            .h_size = 720,                              \
            .v_size = 720,                              \
            .hsync_back_porch = 30,                     \
            .hsync_pulse_width = 24,                    \
            .hsync_front_porch = 30,                    \
            .vsync_back_porch = 16,                     \
            .vsync_pulse_width = 2,                     \
            .vsync_front_porch = 12,                    \
        },                                              \
        JD9365_LCD_DPI_CONFIG_DMA2D                     \
    }
#endif

#ifdef __cplusplus
}
#endif
