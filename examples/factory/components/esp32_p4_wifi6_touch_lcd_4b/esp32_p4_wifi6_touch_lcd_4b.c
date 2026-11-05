#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_spiffs.h"
#include "esp_ldo_regulator.h"
#include "esp_vfs_fat.h"
#include "usb/usb_host.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "esp_board_manager.h"
#include "esp_board_device.h"
#include "esp_board_periph.h"
#include "dev_display_lcd.h"
#include "dev_lcd_touch.h"
#include "dev_audio_codec.h"
#include "periph_i2s.h"
#include "periph_ledc.h"
#include "lcd_panel_probe.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_4b.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "esp_lcd_touch_gt911.h"
#include "bsp_err_check.h"
#include "esp_codec_dev_defaults.h"

static const char *TAG = "ESP32_P4_4B";
typedef enum {
    BSP_PANEL_UNKNOWN,
    BSP_PANEL_JD9365,
    BSP_PANEL_ST7703,
} bsp_panel_kind_t;
static bsp_panel_kind_t display_panel_kind = BSP_PANEL_UNKNOWN;
static bool display_panel_identified = false;
static dev_display_lcd_handles_t *display_handles;
static dev_lcd_touch_handles_t *touch_handles;
static periph_ledc_handle_t *brightness_handle;

const char *bsp_display_get_panel_name(void)
{
    switch (display_panel_kind) {
    case BSP_PANEL_JD9365: return "JD9365DA";
    case BSP_PANEL_ST7703: return "ST7703";
    default: return "unknown";
    }
}

bool bsp_display_panel_is_identified(void)
{
    return display_panel_identified;
}

float bsp_display_get_pixel_clock_mhz(void)
{
    waveshare_lcd_panel_timing_t timing;
    if (waveshare_lcd_panel_get_effective_timing(&timing)) {
        return timing.pixel_clock_mhz;
    }
    return display_panel_kind == BSP_PANEL_ST7703 ? 38.0f : (240.0f / 7.0f);
}

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
static lv_indev_t *disp_indev = NULL;
#endif // (BSP_CONFIG_NO_GRAPHIC_LIB == 0)

sdmmc_card_t *bsp_sdcard = NULL;    // Global uSD card handler
static bool i2c_initialized = false;
static TaskHandle_t usb_host_task;  // USB Host Library task
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0))
static i2c_master_bus_handle_t i2c_handle = NULL;  // I2C Handle
#endif
typedef enum {
    BSP_AUDIO_MODE_NONE,
    BSP_AUDIO_MODE_STD_STD,
    BSP_AUDIO_MODE_TX_STD_RX_TDM,
} bsp_audio_mode_t;

static bsp_audio_mode_t audio_mode = BSP_AUDIO_MODE_NONE;
static i2s_chan_handle_t i2s_tx_chan = NULL;
static i2s_chan_handle_t i2s_rx_chan = NULL;
static esp_codec_dev_handle_t speaker_codec_dev = NULL;
static dev_audio_codec_handles_t *speaker_codec_handles = NULL;

static esp_codec_dev_handle_t microphone_codec_dev = NULL;
static dev_audio_codec_handles_t *microphone_codec_handles = NULL;

/* Can be used for I2S STD and TDM GPIO configuration */
#define BSP_I2S_GPIO_CFG       \
    {                          \
        .mclk = BSP_I2S_MCLK,  \
        .bclk = BSP_I2S_SCLK,  \
        .ws = BSP_I2S_LCLK,    \
        .dout = BSP_I2S_DOUT,  \
        .din = BSP_I2S_DSIN,   \
        .invert_flags = {      \
            .mclk_inv = false, \
            .bclk_inv = false, \
            .ws_inv = false,   \
        },                     \
    }

/* This configuration is used by default in bsp_audio_init() */
#define BSP_I2S_DUPLEX_MONO_CFG(_sample_rate)                                                         \
    {                                                                                                 \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                                          \
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), \
        .gpio_cfg = BSP_I2S_GPIO_CFG,                                                                 \
    }

esp_err_t bsp_i2c_init(void)
{
    /* I2C was initialized before */
    if (i2c_initialized) {
        return ESP_OK;
    }

    BSP_ERROR_CHECK_RETURN_ERR(esp_board_periph_ref_handle("i2c_master", (void **)&i2c_handle));

    i2c_initialized = true;

    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    if (!i2c_initialized) {
        return ESP_OK;
    }
    BSP_ERROR_CHECK_RETURN_ERR(esp_board_periph_unref_handle("i2c_master"));
    i2c_handle = NULL;
    i2c_initialized = false;
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    return i2c_handle;
}

static esp_err_t bsp_enable_ldo_vo4(void)
{
    static esp_ldo_channel_handle_t vo4_chan = NULL;
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = 4,
        .voltage_mv = 3300,
    };

    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &vo4_chan), TAG, "Acquire LDO VO4 channel failed");
    ESP_LOGI(TAG, "LDO VO4 set to 3300mV");

    return ESP_OK;
}

esp_err_t bsp_sdcard_mount(void)
{
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 5,
        .allocation_unit_size = 64 * 1024
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;


    ESP_RETURN_ON_ERROR(bsp_enable_ldo_vo4(), TAG, "DSI PHY power failed");

    const sdmmc_slot_config_t slot_config = {
        /* SD card is connected to Slot 0 pins. Slot 0 uses IO MUX, so not specifying the pins here */
        .cd = SDMMC_SLOT_NO_CD,
        .wp = SDMMC_SLOT_NO_WP,
        .width = 4,
        .flags = 0,
    };



    return esp_vfs_fat_sdmmc_mount(BSP_SD_MOUNT_POINT, &host, &slot_config, &mount_config, &bsp_sdcard);
}

esp_err_t bsp_sdcard_unmount(void)
{
    return esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, bsp_sdcard);
}

esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_BSP_SPIFFS_MOUNT_POINT,
        .partition_label = CONFIG_BSP_SPIFFS_PARTITION_LABEL,
        .max_files = CONFIG_BSP_SPIFFS_MAX_FILES,
#ifdef CONFIG_BSP_SPIFFS_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf);

    BSP_ERROR_CHECK_RETURN_ERR(ret_val);

    size_t total = 0, used = 0;
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ret_val;
}

esp_err_t bsp_spiffs_unmount(void)
{
    return esp_vfs_spiffs_unregister(CONFIG_BSP_SPIFFS_PARTITION_LABEL);
}

/**************************************************************************************************
 *
 * I2S Audio Function
 *
 **************************************************************************************************/
static void bsp_audio_update_result(esp_err_t *result, esp_err_t error)
{
    if ((*result == ESP_OK) && (error != ESP_OK)) {
        *result = error;
    }
}

static bool bsp_audio_has_resources(void)
{
    return (audio_mode != BSP_AUDIO_MODE_NONE) ||
           (i2s_tx_chan != NULL) ||
           (i2s_rx_chan != NULL) ||
           (speaker_codec_dev != NULL) ||
           (speaker_codec_handles != NULL) ||
           (microphone_codec_dev != NULL) ||
           (microphone_codec_handles != NULL);
}

static esp_err_t bsp_audio_deinit_codec_device(const char *name,
                                               dev_audio_codec_handles_t **handles,
                                               esp_codec_dev_handle_t *codec_dev)
{
    esp_err_t ret = ESP_OK;

    if (*handles != NULL) {
        /* The Board Manager owns this codec and every interface below it. */
        bsp_audio_update_result(&ret, esp_board_manager_deinit_device_by_name(name));
        if (ret == ESP_OK) {
            *handles = NULL;
            *codec_dev = NULL;
        }
    }

    return ret;
}

static esp_err_t bsp_audio_release_i2s_channel(i2s_chan_handle_t *channel, const char *name)
{
    if (*channel == NULL) {
        return ESP_OK;
    }
    esp_err_t ret = esp_board_periph_unref_handle(name);
    *channel = NULL;
    return ret;
}

esp_err_t bsp_audio_deinit(void)
{
    esp_err_t ret = ESP_OK;

    /* Devices release their Board Manager I2S references before ours. */
    bsp_audio_update_result(&ret, bsp_audio_deinit_codec_device("audio_adc",
                                                                 &microphone_codec_handles,
                                                                 &microphone_codec_dev));
    bsp_audio_update_result(&ret, bsp_audio_deinit_codec_device("audio_dac",
                                                                 &speaker_codec_handles,
                                                                 &speaker_codec_dev));

    bsp_audio_update_result(&ret, bsp_audio_release_i2s_channel(&i2s_rx_chan, "i2s_audio_in"));
    bsp_audio_update_result(&ret, bsp_audio_release_i2s_channel(&i2s_tx_chan, "i2s_audio_out"));
    audio_mode = BSP_AUDIO_MODE_NONE;

    return ret;
}

static esp_err_t bsp_audio_init_channels(const i2s_std_config_t *tx_config,
                                         const i2s_std_config_t *rx_std_config,
                                         const i2s_tdm_config_t *rx_tdm_config,
                                         bsp_audio_mode_t mode)
{
    esp_err_t ret = ESP_OK;
    esp_err_t cleanup_ret;

    if (bsp_audio_has_resources()) {
        return ESP_ERR_INVALID_STATE;
    }

    periph_i2s_config_t tx_periph_config = {
        .port = CONFIG_BSP_I2S_NUM,
        .role = I2S_ROLE_MASTER,
        .mode = I2S_COMM_MODE_STD,
        .direction = I2S_DIR_TX,
        .i2s_cfg.std = *tx_config,
    };
    periph_i2s_config_t rx_periph_config = {
        .port = CONFIG_BSP_I2S_NUM,
        .role = I2S_ROLE_MASTER,
        .mode = rx_tdm_config ? I2S_COMM_MODE_TDM : I2S_COMM_MODE_STD,
        .direction = I2S_DIR_RX,
    };
    if (rx_tdm_config != NULL) {
        rx_periph_config.i2s_cfg.tdm = *rx_tdm_config;
    } else {
        rx_periph_config.i2s_cfg.std = *rx_std_config;
    }
    ESP_GOTO_ON_ERROR(esp_board_periph_override_config("i2s_audio_out", &tx_periph_config,
                                                        sizeof(tx_periph_config)),
                      err, TAG, "Override I2S TX configuration failed");
    ESP_GOTO_ON_ERROR(esp_board_periph_override_config("i2s_audio_in", &rx_periph_config,
                                                        sizeof(rx_periph_config)),
                      err, TAG, "Override I2S RX configuration failed");
    /* Board Manager's shared-port implementation requires STD TX first, then RX. */
    ESP_GOTO_ON_ERROR(esp_board_periph_ref_handle("i2s_audio_out", (void **)&i2s_tx_chan),
                      err, TAG, "I2S TX initialization failed");
    ESP_GOTO_ON_ERROR(esp_board_periph_ref_handle("i2s_audio_in", (void **)&i2s_rx_chan),
                      err, TAG, "I2S RX initialization failed");

    audio_mode = mode;
    return ESP_OK;

err:
    cleanup_ret = bsp_audio_deinit();
    if (cleanup_ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio cleanup failed: %s", esp_err_to_name(cleanup_ret));
    }
    return ret;
}

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    if ((audio_mode == BSP_AUDIO_MODE_STD_STD) &&
            (i2s_tx_chan != NULL) && (i2s_rx_chan != NULL)) {
        return ESP_OK;
    }
    if (bsp_audio_has_resources()) {
        return ESP_ERR_INVALID_STATE;
    }

    const i2s_std_config_t std_cfg_default = BSP_I2S_DUPLEX_MONO_CFG(22050);
    const i2s_std_config_t *std_config = (i2s_config != NULL) ? i2s_config : &std_cfg_default;

    return bsp_audio_init_channels(std_config, std_config, NULL, BSP_AUDIO_MODE_STD_STD);
}

esp_err_t bsp_audio_init_tx_std_rx_tdm(const i2s_std_config_t *tx_config,
                                       const i2s_tdm_config_t *rx_config)
{
    if (bsp_audio_has_resources()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (tx_config == NULL || rx_config == NULL ||
            tx_config->clk_cfg.sample_rate_hz != rx_config->clk_cfg.sample_rate_hz) {
        return ESP_ERR_INVALID_ARG;
    }

    return bsp_audio_init_channels(tx_config, NULL, rx_config, BSP_AUDIO_MODE_TX_STD_RX_TDM);
}

esp_err_t bsp_audio_init_voice_24k(void)
{
    i2s_std_config_t tx_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(24000),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = BSP_I2S_GPIO_CFG,
    };
    i2s_tdm_config_t rx_config = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(24000),
        .slot_cfg = I2S_TDM_PHILIP_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO,
            I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3),
        .gpio_cfg = BSP_I2S_GPIO_CFG,
    };

    tx_config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    tx_config.gpio_cfg.din = I2S_GPIO_UNUSED;
    rx_config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    rx_config.clk_cfg.bclk_div = 8;
    rx_config.slot_cfg.total_slot = BSP_AUDIO_TDM_SLOT_COUNT;
    rx_config.gpio_cfg.dout = I2S_GPIO_UNUSED;

    return bsp_audio_init_tx_std_rx_tdm(&tx_config, &rx_config);
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    esp_err_t ret;

    if (speaker_codec_dev != NULL) {
        return speaker_codec_dev;
    }

    if (i2s_tx_chan == NULL || i2s_rx_chan == NULL) {
        BSP_ERROR_CHECK_RETURN_NULL(bsp_audio_init(NULL));
    }

    ret = esp_board_manager_init_device_by_name("audio_dac");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio DAC initialization failed: %s", esp_err_to_name(ret));
        return NULL;
    }
    ret = esp_board_manager_get_device_handle("audio_dac", (void **)&speaker_codec_handles);
    if (ret != ESP_OK ||
        speaker_codec_handles == NULL || speaker_codec_handles->codec_dev == NULL) {
        ESP_LOGE(TAG, "Audio DAC handle retrieval failed");
        (void)esp_board_manager_deinit_device_by_name("audio_dac");
        speaker_codec_handles = NULL;
        return NULL;
    }
    speaker_codec_dev = speaker_codec_handles->codec_dev;

    return speaker_codec_dev;
}

esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    esp_err_t ret;

    if (microphone_codec_dev != NULL) {
        return microphone_codec_dev;
    }

    if (i2s_tx_chan == NULL || i2s_rx_chan == NULL) {
        BSP_ERROR_CHECK_RETURN_NULL(bsp_audio_init(NULL));
    }

    ret = esp_board_manager_init_device_by_name("audio_adc");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio ADC initialization failed: %s", esp_err_to_name(ret));
        return NULL;
    }
    ret = esp_board_manager_get_device_handle("audio_adc", (void **)&microphone_codec_handles);
    if (ret != ESP_OK ||
        microphone_codec_handles == NULL || microphone_codec_handles->codec_dev == NULL) {
        ESP_LOGE(TAG, "Audio ADC handle retrieval failed");
        (void)esp_board_manager_deinit_device_by_name("audio_adc");
        microphone_codec_handles = NULL;
        return NULL;
    }
    microphone_codec_dev = microphone_codec_handles->codec_dev;

    return microphone_codec_dev;
}

// Bit number used to represent command and parameter
#define LCD_LEDC_CH            CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH

esp_err_t bsp_display_brightness_init(void)
{
    if (brightness_handle != NULL) {
        return ESP_OK;
    }
    BSP_ERROR_CHECK_RETURN_ERR(esp_board_manager_init_device_by_name("lcd_brightness"));
    BSP_ERROR_CHECK_RETURN_ERR(esp_board_manager_get_device_handle("lcd_brightness",
                                                                     (void **)&brightness_handle));

    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    } else if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);

    BSP_ERROR_CHECK_RETURN_ERR(bsp_display_brightness_init());
    uint32_t duty_cycle = (1023 * brightness_percent) / 100;
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(brightness_handle->speed_mode, brightness_handle->channel, duty_cycle));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_update_duty(brightness_handle->speed_mode, brightness_handle->channel));

    return ESP_OK;
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;
    bsp_lcd_handles_t handles;
    ret = bsp_display_new_with_handles(config, &handles);

    *ret_panel = handles.panel;
    *ret_io = handles.io;

    return ret;
}

esp_err_t bsp_display_new_with_handles(const bsp_display_config_t *config, bsp_lcd_handles_t *ret_handles)
{
    ESP_RETURN_ON_FALSE(ret_handles, ESP_ERR_INVALID_ARG, TAG, "invalid display handles");
    *ret_handles = (bsp_lcd_handles_t){0};
    (void)config;
    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Brightness init failed");
    if (display_handles == NULL) {
        ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name("display_lcd"), TAG, "Display init failed");
        ESP_RETURN_ON_ERROR(esp_board_manager_get_device_handle("display_lcd", (void **)&display_handles), TAG,
                            "Display handle missing");
        switch (waveshare_lcd_panel_get_kind()) {
        case WAVESHARE_LCD_PANEL_JD9365: display_panel_kind = BSP_PANEL_JD9365; break;
        case WAVESHARE_LCD_PANEL_ST7703: display_panel_kind = BSP_PANEL_ST7703; break;
        default: display_panel_kind = BSP_PANEL_UNKNOWN; break;
        }
        display_panel_identified = waveshare_lcd_panel_is_identified();
    }
    ret_handles->io = display_handles->io_handle;
    ret_handles->panel = display_handles->panel_handle;
    ESP_LOGI(TAG, "Display initialized");
    return ESP_OK;
}

esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    const bsp_touch_config_t default_config = {
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    if (config == NULL) {
        config = &default_config;
    }

    if (touch_handles == NULL) {
        dev_lcd_touch_config_t *touch_cfg = NULL;
        ESP_RETURN_ON_ERROR(esp_board_manager_get_device_config("lcd_touch", (void **)&touch_cfg), TAG,
                            "Touch configuration missing");
        dev_lcd_touch_config_t adjusted_cfg = *touch_cfg;
        adjusted_cfg.touch_config.flags.swap_xy = config->flags.swap_xy;
        adjusted_cfg.touch_config.flags.mirror_x = config->flags.mirror_x;
        adjusted_cfg.touch_config.flags.mirror_y = config->flags.mirror_y;
        ESP_RETURN_ON_ERROR(esp_board_device_override_config("lcd_touch", &adjusted_cfg, sizeof(adjusted_cfg)), TAG,
                            "Touch configuration override failed");
        ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name("lcd_touch"), TAG, "Touch init failed");
        ESP_RETURN_ON_ERROR(esp_board_manager_get_device_handle("lcd_touch", (void **)&touch_handles), TAG,
                            "Touch handle missing");
    }
    *ret_touch = touch_handles->touch_handle;
    return ESP_OK;
}

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
static lv_display_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    bsp_lcd_handles_t lcd_panels;
    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_new_with_handles(NULL, &lcd_panels));

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const esp_lv_adapter_display_config_t disp_cfg = {
        .panel = lcd_panels.panel,
        .panel_io = lcd_panels.io,
        .profile = {
            .interface = ESP_LV_ADAPTER_PANEL_IF_MIPI_DSI,
            .rotation = cfg->rotation,
            .hor_res = BSP_LCD_H_RES,
            .ver_res = BSP_LCD_V_RES,
            .buffer_height = 50,
            .use_psram = false,
            .enable_ppa_accel = false,
            .require_double_buffer = false,
        },
        .tear_avoid_mode = cfg->tear_avoid_mode,
    };

    return esp_lv_adapter_register_display(&disp_cfg);
}

static lv_indev_t *bsp_display_indev_init(const bsp_display_cfg_t *cfg, lv_display_t *disp)
{
    assert(cfg != NULL);
    esp_lcd_touch_handle_t tp;
    const bsp_touch_config_t touch_config = {
        .flags = {
            .swap_xy = cfg->touch_flags.swap_xy,
            .mirror_x = cfg->touch_flags.mirror_x,
            .mirror_y = cfg->touch_flags.mirror_y,
        },
    };
    BSP_ERROR_CHECK_RETURN_NULL(bsp_touch_new(&touch_config, &tp));
    assert(tp);

    /* Add touch input (for selected screen) */
    const esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, tp);

    return esp_lv_adapter_register_touch(&touch_cfg);
}

lv_display_t *bsp_display_start(void)
{
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    return bsp_display_start_with_config(&cfg);
}

lv_display_t *bsp_display_start_with_config(bsp_display_cfg_t *cfg)
{
    lv_display_t *disp;

    assert(cfg != NULL);
    BSP_ERROR_CHECK_RETURN_NULL(esp_lv_adapter_init(&cfg->lv_adapter_cfg));

    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_brightness_init());

    BSP_NULL_CHECK(disp = bsp_display_lcd_init(cfg), NULL);

    BSP_NULL_CHECK(disp_indev = bsp_display_indev_init(cfg, disp), NULL);

    ESP_ERROR_CHECK(esp_lv_adapter_start());

    return disp;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return disp_indev;
}

void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation)
{
    lv_disp_set_rotation(disp, rotation);
}

bool bsp_display_lock(int32_t timeout_ms)
{
    return esp_lv_adapter_lock(timeout_ms) == ESP_OK;
}

void bsp_display_unlock(void)
{
    esp_lv_adapter_unlock();
}

#endif // (BSP_CONFIG_NO_GRAPHIC_LIB == 0)

static void usb_lib_task(void *arg)
{
    while (1) {
        // Start handling system events
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "USB: All devices freed");
        }
    }
}

esp_err_t bsp_usb_host_start(bsp_usb_host_power_mode_t mode, bool limit_500mA)
{
    //Install USB Host driver. Should only be called once in entire application
    ESP_LOGI(TAG, "Installing USB Host");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    BSP_ERROR_CHECK_RETURN_ERR(usb_host_install(&host_config));

    // Create a task that will handle USB library events
    if (xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, 10, &usb_host_task) != pdTRUE) {
        ESP_LOGE(TAG, "Creating USB host lib task failed");
        abort();
    }

    return ESP_OK;
}

esp_err_t bsp_usb_host_stop(void)
{
    usb_host_uninstall();
    if (usb_host_task) {
        vTaskSuspend(usb_host_task);
        vTaskDelete(usb_host_task);
    }
    return ESP_OK;
}
