# ESP LCD JD9365DA

Local MIPI-DSI panel driver for the 4-inch 720x720 JD9365DA panel used as the replacement screen of the
ESP32-P4-WIFI6-Touch-LCD-4B. It lives in the project component directory instead of the ESP Component
Registry because the initialization list is specific to this panel.

| LCD controller | Communication interface | Component name  |
| :------------: | :---------------------: | :-------------: |
|    JD9365DA    |        MIPI-DSI         | esp_lcd_jd9365  |

**Note**: the MIPI-DSI interface requires ESP-IDF v5.3 or later.

## Panel parameters

| Parameter | Value |
| --- | --- |
| Resolution | 720 x 720 |
| MIPI-DSI lanes | 2 |
| Lane bit rate | 480 Mbps |
| Pixel clock | 240 / 7 = 34.286 MHz (see below) |
| HSYNC / HBP / HFP | 24 / 30 / 30 |
| VSYNC / VBP / VFP | 2 / 16 / 12 |
| Refresh rate | 56.9 Hz |
| Touch controller | GT911 (handled by the board BSP) |

The ESP32-P4 DPI clock comes from PLL_F240M (240 MHz) through an integer divider, so the vendor's 36 MHz is
not reachable. The HAL compensates the difference by shrinking the line length, and that compensation lands
on the horizontal front porch: `804 * (240 / 7) / 36 - 804 = -38`, so HFP would become `30 - 38 = -8`. The
DSI bridge then produces no valid stream (the panel shows a solid blue screen) and
`esp_lcd_panel_draw_bitmap()` stops signalling frame completion, deadlocking every LVGL flush. Requesting
the achievable clock `240 / 7 = 34.286 MHz` keeps the compensation at exactly zero, so the vendor blanking
is programmed verbatim and the panel runs at 56.9 Hz.

`JD9365_720_720_PANEL_60HZ_DPI_CONFIG(px_format)` expands to the matching `esp_lcd_dpi_panel_config_t`.

## Initialization sequence

The built-in sequence is the 8904ANT vendor register list documented in
`docs/display/8904ANT------code .txt` with one change: page 0 register `0x80` is written as `0x01`
instead of `0x03`, because the ESP32-P4 DSI host drives the panel with two data lanes. `0x11` (sleep
out, 120 ms) and `0x29` (display on, 20 ms) are appended by the driver because the vendor list ends
after the manufacturer page setup.

`vendor_config.mipi_config.lane_num` programs register `0x80` before the vendor list runs, so a custom
sequence does not have to repeat it. Pass `init_cmds`/`init_cmds_size` in the vendor configuration to
replace the built-in list for a different panel.

## Example use

```c
esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
esp_lcd_dsi_bus_config_t bus_config = JD9365_PANEL_BUS_DSI_2CH_CONFIG();
ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

esp_lcd_panel_io_handle_t io;
esp_lcd_dbi_io_config_t dbi_config = JD9365_PANEL_IO_DBI_CONFIG();
ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io));

esp_lcd_dpi_panel_config_t dpi_config = JD9365_720_720_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
jd9365_vendor_config_t vendor_config = {
    .flags = {
        .use_mipi_interface = 1,
    },
    .mipi_config = {
        .dsi_bus = mipi_dsi_bus,
        .dpi_config = &dpi_config,
        .lane_num = 2,
    },
};
esp_lcd_panel_dev_config_t panel_config = {
    .reset_gpio_num = -1,
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = 16,
    .vendor_config = &vendor_config,
};
esp_lcd_panel_handle_t panel;
ESP_ERROR_CHECK(esp_lcd_new_panel_jd9365(io, &panel_config, &panel));
ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
```
