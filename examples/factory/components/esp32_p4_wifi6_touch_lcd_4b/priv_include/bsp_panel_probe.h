/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"

typedef enum {
    BSP_PANEL_UNKNOWN,
    BSP_PANEL_JD9365,
    BSP_PANEL_ST7703,
} bsp_panel_kind_t;

/* Only for this board's DSI bus 0, virtual channel 0, before video starts. */
void bsp_panel_io_enable_bounded_reads(esp_lcd_panel_io_handle_t io);
esp_err_t bsp_panel_probe(esp_lcd_panel_io_handle_t io, bsp_panel_kind_t *kind);
