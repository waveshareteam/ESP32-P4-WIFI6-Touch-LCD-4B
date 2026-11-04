# BSP: Waveshare ESP32-P4-WIFI6-Touch-LCD-4B (local dual-panel BSP)

Local fork of `waveshare/esp32_p4_wifi6_touch_lcd_4b` 3.0.1 living in
`examples/factory/components/`. It is used by the Brookesia project instead of the Registry package so
that the replacement 4-inch 720x720 panel driven by a JD9365DA can be supported. The two components must
never be linked together: both provide the same `bsp/*` headers and symbols.

This series of products is based on ESP32-P4 chip high-performance development board. It uses dual-core plus single-core RISC-V architecture design, and a 4-inch 720 × 720 resolution IPS touch display is mounted on the board
| HW version | BSP Version |
| :--------: | :---------: |
|    [V1.0](http://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B)    |      ^1     |


## Panel selection

`CONFIG_BSP_LCD_PANEL_IC` chooses the controller of the 720x720 MIPI-DSI panel:

| Option | Panel | Driver | Timing |
| --- | --- | --- | --- |
| `BSP_LCD_PANEL_IC_JD9365DA` | Replacement 4-inch panel | local `esp_lcd_jd9365` | 2 lanes, 480 Mbps, 34.286 MHz PCLK, HSYNC/HBP/HFP 24/30/30, VSYNC/VBP/VFP 2/16/12 |
| `BSP_LCD_PANEL_IC_ST7703` | Stock 4-inch panel | Registry `waveshare/esp_lcd_st7703` | 2 lanes, 480 Mbps, 38 MHz PCLK, HSYNC/HBP/HFP 20/50/50, VSYNC/VBP/VFP 4/20/20 |

`BSP_LCD_PANEL_IC_AUTO` is the default. After a hardware reset, it reads DCS `0x04` and recognizes
the ST7703 reset ID `38 21 1F`. Otherwise it unlocks the JD9365DA command pages and verifies the
fixed chip production number `93 65` at page 1 registers `0x8D/0x8E`. Standard DCS module IDs are
OTP-programmable and are not assumed to equal the controller's model number. DSI reads have a
100 ms deadline; after probing, the host is recreated and the panel is reset before initialization.
An unknown ID or a read timeout logs a warning and selects an **unconfirmed** JD9365DA profile.
The manual choices support modules whose programmed IDs differ. `bsp_display_get_panel_name()` and
`bsp_display_panel_is_identified()` report the selected profile and whether detection confirmed it.

Both panels use the same board wiring (`BSP_LCD_RST` on GPIO27, backlight on GPIO26, DSI PHY LDO VO3 at
2500 mV, DSI PHY/panel LDO VO4 at 3300 mV) and the same GT911 touch path. The JD9365DA initialization
list is the 8904ANT vendor sequence with page 0 register `0x80 = 0x01` for two data lanes; see
[the JD9365DA driver](../esp_lcd_jd9365/README.md).

## BackLight
```c
bsp_display_brightness_init();

bsp_display_backlight_on();

bsp_display_backlight_off();

bsp_display_brightness_set(100);
```

## Touch

The GT911 reset uses GPIO23; its INT signal is not connected to the MCU. The BSP probes
both valid I2C addresses (`0x5D`, then `0x14`) and creates the touch panel I/O
with the address that responds. With no interrupt GPIO configured, touch data is
read by polling.

## Capabilities and dependencies

|  Capability |     Available    | Component                                                                                                  |
|-------------|------------------|------------------------------------------------------------------------------------------------------------|
|   DISPLAY   |:heavy_check_mark:| local [esp_lcd_jd9365](../esp_lcd_jd9365/README.md) or [waveshare/esp_lcd_st7703](https://components.espressif.com/components/waveshare/esp_lcd_st7703) |
| LVGL_ADAPTER|:heavy_check_mark:| [espressif/esp_lvgl_adapter](https://components.espressif.com/components/espressif/esp_lvgl_adapter)       |
|    TOUCH    |:heavy_check_mark:| [espressif/esp_lcd_touch_gt911](https://components.espressif.com/components/espressif/esp_lcd_touch_gt911) |
|   BUTTONS   |        :x:       |                                                                                                            |
|    AUDIO    |:heavy_check_mark:| [espressif/esp_codec_dev](https://components.espressif.com/components/espressif/esp_codec_dev)             |
|AUDIO_SPEAKER|:heavy_check_mark:|                                                                                                            |
|  AUDIO_MIC  |:heavy_check_mark:|                                                                                                            |
|    SDCARD   |:heavy_check_mark:| idf                                                                                                        |
|     IMU     |        :x:       |                                                                                                            |
