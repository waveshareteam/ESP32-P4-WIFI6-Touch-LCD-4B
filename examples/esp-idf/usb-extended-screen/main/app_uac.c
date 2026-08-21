/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include "app_usb.h"
#include "esp_check.h"
#include "esp_log.h"
#include "usb_device_uac.h"
#include "usb_descriptors.h"
#include "bsp_board_extra.h"


static const char *TAG = "app_uac";

static esp_err_t uac_device_output_cb(uint8_t *buf, size_t len, void *arg)
{
    (void)arg;
    size_t bytes_written = 0;
    esp_err_t result = bsp_extra_i2s_write(buf, len, &bytes_written, 0);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(result));
        return result;
    }
    if (bytes_written != len) {
        ESP_LOGE(TAG, "short I2S write: %u of %u bytes", (unsigned)bytes_written, (unsigned)len);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t uac_device_input_cb(uint8_t *buf, size_t len, size_t *bytes_read, void *arg)
{
    (void)arg;
    esp_err_t result = bsp_extra_i2s_read(buf, len, bytes_read, 0);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "I2S read failed: %s", esp_err_to_name(result));
    }
    return result;
}

static void uac_device_set_mute_cb(uint32_t mute, void *arg)
{
    (void)arg;
    ESP_LOGD(TAG, "uac_device_set_mute_cb: %"PRIu32"", mute);
    if (bsp_extra_codec_mute_set(mute != 0) != ESP_OK) {
        ESP_LOGE(TAG, "codec mute update failed");
    }
}

static void uac_device_set_volume_cb(uint32_t volume, void *arg)
{
    (void)arg;
    ESP_LOGD(TAG, "uac_device_set_volume_cb: %"PRIu32"", volume);
    if (bsp_extra_codec_volume_set((int)volume, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "codec volume update failed");
    }
}

esp_err_t app_uac_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_extra_codec_init(), TAG, "codec init failed");
    ESP_RETURN_ON_ERROR(bsp_extra_codec_set_fs(CONFIG_UAC_SAMPLE_RATE,
                                               CONFIG_UAC_BYTES_PER_SAMPLE * 8,
                                               CONFIG_UAC_SPEAKER_CHANNEL_NUM),
                        TAG, "codec sample format config failed");

    uac_device_config_t config = {
        .skip_tinyusb_init = true,
        .output_cb = uac_device_output_cb,
        .input_cb = uac_device_input_cb,
        .set_mute_cb = uac_device_set_mute_cb,
        .set_volume_cb = uac_device_set_volume_cb,
        .cb_ctx = NULL,
#if CONFIG_UAC_SPEAKER_CHANNEL_NUM > 0
        .spk_itf_num = ITF_NUM_AUDIO_STREAMING_SPK,
#endif
#if CONFIG_UAC_MIC_CHANNEL_NUM > 0
        .mic_itf_num = ITF_NUM_AUDIO_STREAMING_MIC,
#endif
    };

    return uac_device_init(&config);
}
