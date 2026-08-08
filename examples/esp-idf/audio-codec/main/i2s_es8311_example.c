/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "example_config.h"

static const char *TAG = "audio_codec";
static esp_codec_dev_handle_t s_playback_dev;
#if CONFIG_EXAMPLE_MODE_ECHO
static esp_codec_dev_handle_t s_record_dev;
#endif

#if CONFIG_EXAMPLE_MODE_MUSIC
extern const uint8_t music_pcm_start[] asm("_binary_canon_pcm_start");
extern const uint8_t music_pcm_end[] asm("_binary_canon_pcm_end");
#endif

static esp_err_t audio_devices_init(void)
{
#if CONFIG_EXAMPLE_MODE_MUSIC
    i2s_std_config_t i2s_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(EXAMPLE_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO
        ),
        .gpio_cfg = {
            .mclk = BSP_I2S_MCLK,
            .bclk = BSP_I2S_SCLK,
            .ws = BSP_I2S_LCLK,
            .dout = BSP_I2S_DOUT,
            .din = BSP_I2S_DSIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    i2s_config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;

    ESP_RETURN_ON_ERROR(bsp_audio_init(&i2s_config), TAG, "initialize BSP audio bus");
#else
    ESP_RETURN_ON_ERROR(bsp_audio_init_voice_24k(), TAG, "initialize BSP 24 kHz voice bus");
#endif

    esp_codec_dev_sample_info_t playback_format = {
        .sample_rate = EXAMPLE_SAMPLE_RATE,
        .channel = EXAMPLE_CHANNEL_COUNT,
        .bits_per_sample = EXAMPLE_BITS_PER_SAMPLE,
    };

    s_playback_dev = bsp_audio_codec_speaker_init();
    ESP_RETURN_ON_FALSE(s_playback_dev, ESP_FAIL, TAG, "initialize ES8311 playback codec");
    ESP_RETURN_ON_ERROR(esp_codec_dev_open(s_playback_dev, &playback_format), TAG, "open playback codec");
    ESP_RETURN_ON_ERROR(
        esp_codec_dev_set_out_vol(s_playback_dev, EXAMPLE_VOICE_VOLUME),
        TAG,
        "set playback volume"
    );
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_mute(s_playback_dev, false), TAG, "unmute playback codec");

#if CONFIG_EXAMPLE_MODE_ECHO
    s_record_dev = bsp_audio_codec_microphone_init();
    ESP_RETURN_ON_FALSE(s_record_dev, ESP_FAIL, TAG, "initialize ES7210 record codec");
    esp_codec_dev_sample_info_t record_format = {
        .sample_rate = EXAMPLE_SAMPLE_RATE,
        .channel = BSP_AUDIO_TDM_SLOT_COUNT,
        .bits_per_sample = EXAMPLE_BITS_PER_SAMPLE,
        .channel_mask = BSP_AUDIO_TDM_SLOT_MASK_FL_FR,
    };
    ESP_RETURN_ON_ERROR(esp_codec_dev_open(s_record_dev, &record_format), TAG, "open record codec");
#endif
    return ESP_OK;
}

#if CONFIG_EXAMPLE_MODE_MUSIC
static void audio_task(void *arg)
{
    (void)arg;
    const size_t music_size = (size_t)(music_pcm_end - music_pcm_start);
    ESP_LOGI(TAG, "Playing %u bytes of 16 kHz stereo PCM", (unsigned)music_size);

    while (true) {
        ESP_ERROR_CHECK(esp_codec_dev_write(s_playback_dev, (void *)music_pcm_start, music_size));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#else
static void audio_task(void *arg)
{
    (void)arg;
    uint8_t *buffer = malloc(EXAMPLE_ECHO_BUFFER_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Unable to allocate echo buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "24 kHz FL/FR microphone echo started");
    while (true) {
        ESP_ERROR_CHECK(esp_codec_dev_read(s_record_dev, buffer, EXAMPLE_ECHO_BUFFER_SIZE));
        ESP_ERROR_CHECK(esp_codec_dev_write(s_playback_dev, buffer, EXAMPLE_ECHO_BUFFER_SIZE));
    }
}
#endif

void app_main(void)
{
    ESP_ERROR_CHECK(audio_devices_init());
    BaseType_t created = xTaskCreate(audio_task, "audio", 4096, NULL, 5, NULL);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_FAIL);
}
