/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2s_std.h"

#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"

static const char *TAG = "bsp_extra_board";

static esp_codec_dev_handle_t play_dev_handle;
static esp_codec_dev_handle_t record_dev_handle;

static bool _is_audio_init = false;
static bool _is_player_init = false;
static int _volume_intensity = CODEC_DEFAULT_VOLUME;

static audio_player_cb_t audio_idle_callback = NULL;
static void *audio_idle_cb_user_data = NULL;
static char audio_file_path[128];

/**************************************************************************************************
 *
 * Extra Board Function
 *
 **************************************************************************************************/

static esp_err_t audio_mute_function(AUDIO_PLAYER_MUTE_SETTING setting)
{
    // Volume saved when muting and restored when unmuting. Restoring volume is necessary
    // as es8311_set_voice_mute(true) results in voice volume (REG32) being set to zero.

    bsp_extra_codec_mute_set(setting == AUDIO_PLAYER_MUTE ? true : false);

    // restore the voice volume upon unmuting
    if (setting == AUDIO_PLAYER_UNMUTE) {
        ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(play_dev_handle, _volume_intensity), TAG, "Set Codec volume failed");
    }

    return ESP_OK;
}

static void audio_callback(audio_player_cb_ctx_t *ctx)
{
    if (audio_idle_callback) {
        ctx->user_ctx = audio_idle_cb_user_data;
        audio_idle_callback(ctx);
    }
}

esp_err_t bsp_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    (void)timeout_ms;
    ESP_RETURN_ON_FALSE(record_dev_handle && audio_buffer && len > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid I2S read arguments");
    if (bytes_read) {
        *bytes_read = 0;
    }
    esp_err_t ret = esp_codec_dev_read(record_dev_handle, audio_buffer, len);
    if (ret == ESP_OK && bytes_read) {
        *bytes_read = len;
    }
    return ret;
}

esp_err_t bsp_extra_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    (void)timeout_ms;
    ESP_RETURN_ON_FALSE(play_dev_handle && audio_buffer && len > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid I2S write arguments");
    if (bytes_written) {
        *bytes_written = 0;
    }
    esp_err_t ret = esp_codec_dev_write(play_dev_handle, audio_buffer, len);
    if (ret == ESP_OK && bytes_written) {
        *bytes_written = len;
    }
    return ret;
}

esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    ESP_RETURN_ON_FALSE(play_dev_handle || record_dev_handle, ESP_ERR_INVALID_STATE, TAG, "Codec is not initialized");

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    if (play_dev_handle) {
        ESP_RETURN_ON_ERROR(esp_codec_dev_close(play_dev_handle), TAG, "Close playback codec failed");
    }
    if (record_dev_handle) {
        ESP_RETURN_ON_ERROR(esp_codec_dev_close(record_dev_handle), TAG, "Close record codec failed");
    }

    if (play_dev_handle) {
        ESP_RETURN_ON_ERROR(esp_codec_dev_open(play_dev_handle, &fs), TAG, "Open playback codec failed");
    }
    if (record_dev_handle) {
        ESP_RETURN_ON_ERROR(esp_codec_dev_open(record_dev_handle, &fs), TAG, "Open record codec failed");
        ESP_RETURN_ON_ERROR(esp_codec_dev_set_in_gain(record_dev_handle, CODEC_DEFAULT_ADC_VOLUME), TAG, "Set record gain failed");
    }
    return ESP_OK;
}

esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set)
{
    ESP_RETURN_ON_FALSE(play_dev_handle, ESP_ERR_INVALID_STATE, TAG, "Playback codec is not initialized");
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(play_dev_handle, volume), TAG, "Set Codec volume failed");
    _volume_intensity = volume;
    if (volume_set) {
        *volume_set = volume;
    }
    ESP_LOGI(TAG, "Setting volume: %d", volume);
    return ESP_OK;
}

int bsp_extra_codec_volume_get(void)
{
    return _volume_intensity;
}

esp_err_t bsp_extra_codec_mute_set(bool enable)
{
    ESP_RETURN_ON_FALSE(play_dev_handle, ESP_ERR_INVALID_STATE, TAG, "Playback codec is not initialized");
    return esp_codec_dev_set_out_mute(play_dev_handle, enable);
}

esp_err_t bsp_extra_codec_dev_stop(void)
{
    esp_err_t ret = ESP_OK;

    if (play_dev_handle) {
        ret = esp_codec_dev_close(play_dev_handle);
    }

    if (record_dev_handle) {
        ret = esp_codec_dev_close(record_dev_handle);
    }
    return ret;
}

esp_err_t bsp_extra_codec_dev_resume(void)
{
    return bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL);
}

esp_err_t bsp_extra_codec_init(void)
{
    if (_is_audio_init) {
        return ESP_OK;
    }

    play_dev_handle = bsp_audio_codec_speaker_init();
    ESP_RETURN_ON_FALSE(play_dev_handle, ESP_FAIL, TAG, "Playback codec initialization failed");

    record_dev_handle = bsp_audio_codec_microphone_init();
    ESP_RETURN_ON_FALSE(record_dev_handle, ESP_FAIL, TAG, "Record codec initialization failed");

    ESP_RETURN_ON_ERROR(bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH,
                                               CODEC_DEFAULT_CHANNEL), TAG, "Default codec format failed");

    _is_audio_init = true;
    return ESP_OK;
}

esp_err_t bsp_extra_player_init(void)
{
    if (_is_player_init) {
        return ESP_OK;
    }

    audio_player_config_t config = { .mute_fn = audio_mute_function,
                                     .write_fn = bsp_extra_i2s_write,
                                     .clk_set_fn = bsp_extra_codec_set_fs,
                                     .priority = 5
                                   };
    ESP_RETURN_ON_ERROR(audio_player_new(config), TAG, "audio_player_init failed");
    audio_player_callback_register(audio_callback, NULL);

    _is_player_init = true;

    return ESP_OK;
}

esp_err_t bsp_extra_player_del(void)
{
    _is_player_init = false;

    ESP_RETURN_ON_ERROR(audio_player_delete(), TAG, "audio_player_delete failed");

    return ESP_OK;
}

esp_err_t bsp_extra_file_instance_init(const char *path, file_iterator_instance_t **ret_instance)
{
    ESP_RETURN_ON_FALSE(path, ESP_FAIL, TAG, "path is NULL");
    ESP_RETURN_ON_FALSE(ret_instance, ESP_FAIL, TAG, "ret_instance is NULL");

    file_iterator_instance_t *file_iterator = file_iterator_new(path);
    ESP_RETURN_ON_FALSE(file_iterator, ESP_FAIL, TAG, "file_iterator_new failed, %s", path);

    *ret_instance = file_iterator;

    return ESP_OK;
}

esp_err_t bsp_extra_player_play_index(file_iterator_instance_t *instance, int index)
{
    ESP_RETURN_ON_FALSE(instance, ESP_FAIL, TAG, "instance is NULL");

    ESP_LOGI(TAG, "play_index(%d)", index);
    char filename[128] = {0};
    int retval = file_iterator_get_full_path_from_index(instance, index, filename, sizeof(filename));
    ESP_RETURN_ON_FALSE(retval != 0, ESP_FAIL, TAG, "file_iterator_get_full_path_from_index failed");

    ESP_LOGI(TAG, "opening file '%s'", filename);
    FILE *fp = fopen(filename, "rb");
    ESP_RETURN_ON_FALSE(fp, ESP_FAIL, TAG, "unable to open file");

    ESP_LOGI(TAG, "Playing '%s'", filename);
    esp_err_t ret = audio_player_play(fp);
    if (ret != ESP_OK) {
        fclose(fp);
        ESP_LOGE(TAG, "audio_player_play failed: %s", esp_err_to_name(ret));
        return ret;
    }

    strlcpy(audio_file_path, filename, sizeof(audio_file_path));
    return ESP_OK;
}

esp_err_t bsp_extra_player_play_file(const char *file_path)
{
    ESP_RETURN_ON_FALSE(file_path, ESP_ERR_INVALID_ARG, TAG, "file_path is NULL");
    ESP_LOGI(TAG, "opening file '%s'", file_path);
    FILE *fp = fopen(file_path, "rb");
    ESP_RETURN_ON_FALSE(fp, ESP_FAIL, TAG, "unable to open file");

    ESP_LOGI(TAG, "Playing '%s'", file_path);
    esp_err_t ret = audio_player_play(fp);
    if (ret != ESP_OK) {
        fclose(fp);
        ESP_LOGE(TAG, "audio_player_play failed: %s", esp_err_to_name(ret));
        return ret;
    }

    strlcpy(audio_file_path, file_path, sizeof(audio_file_path));
    return ESP_OK;
}

void bsp_extra_player_register_callback(audio_player_cb_t cb, void *user_data)
{
    audio_idle_callback = cb;
    audio_idle_cb_user_data = user_data;
}

bool bsp_extra_player_is_playing_by_path(const char *file_path)
{
    return file_path && (strcmp(audio_file_path, file_path) == 0);
}

bool bsp_extra_player_is_playing_by_index(file_iterator_instance_t *instance, int index)
{
    return instance && (index == file_iterator_get_index(instance));
}