/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "usb_frame.h"
#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
#include "driver/jpeg_decode.h"
#else
#include "esp_heap_caps.h"
#endif

static QueueHandle_t empty_fb_queue = NULL;
static QueueHandle_t filled_fb_queue = NULL;
static const char *TAG = "usb_frame";

static void frame_free_queue(QueueHandle_t *queue)
{
    if (!queue || !*queue) {
        return;
    }

    frame_t *frame = NULL;
    while (xQueueReceive(*queue, &frame, 0) == pdPASS) {
        if (frame) {
            free(frame->data);
            free(frame);
        }
    }
    vQueueDelete(*queue);
    *queue = NULL;
}

static void frame_release_all(void)
{
    frame_free_queue(&filled_fb_queue);
    frame_free_queue(&empty_fb_queue);
}

esp_err_t frame_allocate(int nb_of_fb, size_t fb_size)
{
    ESP_RETURN_ON_FALSE(nb_of_fb > 0 && fb_size > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid frame allocation size");
    ESP_RETURN_ON_FALSE(!empty_fb_queue && !filled_fb_queue, ESP_ERR_INVALID_STATE, TAG, "Frame queues already allocated");

    // We will be passing the frame buffers by reference.
    empty_fb_queue = xQueueCreate(nb_of_fb, sizeof(frame_t *));
    ESP_RETURN_ON_FALSE(empty_fb_queue, ESP_ERR_NO_MEM, TAG, "Not enough memory for empty_fb_queue %d", nb_of_fb);

    filled_fb_queue = xQueueCreate(nb_of_fb, sizeof(frame_t *));
    if (!filled_fb_queue) {
        frame_release_all();
        ESP_LOGE(TAG, "Not enough memory for filled_fb_queue %d", nb_of_fb);
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < nb_of_fb; i++) {
        frame_t *this_fb = calloc(1, sizeof(frame_t));
        if (!this_fb) {
            frame_release_all();
            ESP_LOGE(TAG, "Not enough memory for frame metadata");
            return ESP_ERR_NO_MEM;
        }

        size_t allocated_size = fb_size;
#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
        jpeg_decode_memory_alloc_cfg_t tx_mem_cfg = {
            .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER,
        };
        uint8_t *this_data = (uint8_t *)jpeg_alloc_decoder_mem(fb_size, &tx_mem_cfg, &allocated_size);
#else
        uint8_t *this_data = (uint8_t *)heap_caps_aligned_alloc(16, fb_size, MALLOC_CAP_SPIRAM);
#endif
        if (!this_data) {
            free(this_fb);
            frame_release_all();
            ESP_LOGE(TAG, "Not enough memory for frame buffer %u", (unsigned)fb_size);
            return ESP_ERR_NO_MEM;
        }

        this_fb->data = this_data;
        this_fb->data_buffer_len = allocated_size;

        if (xQueueSend(empty_fb_queue, &this_fb, 0) != pdPASS) {
            free(this_data);
            free(this_fb);
            frame_release_all();
            ESP_LOGE(TAG, "Queueing an empty frame failed");
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

void frame_reset(frame_t *frame)
{
    if (frame) {
        frame->data_len = 0;
    }
}

esp_err_t frame_return_empty(frame_t *frame)
{
    ESP_RETURN_ON_FALSE(frame && empty_fb_queue, ESP_ERR_INVALID_ARG, TAG, "Invalid empty frame");
    frame_reset(frame);
    BaseType_t result = xQueueSend(empty_fb_queue, &frame, 0);
    ESP_RETURN_ON_FALSE(result == pdPASS, ESP_ERR_NO_MEM, TAG, "Not enough memory empty_fb_queue");
    return ESP_OK;
}

esp_err_t frame_send_filled(frame_t *frame)
{
    ESP_RETURN_ON_FALSE(frame && filled_fb_queue, ESP_ERR_INVALID_ARG, TAG, "Invalid filled frame");
    BaseType_t result = xQueueSend(filled_fb_queue, &frame, 0);
    ESP_RETURN_ON_FALSE(result == pdPASS, ESP_ERR_NO_MEM, TAG, "Not enough memory filled_fb_queue");
    return ESP_OK;
}

esp_err_t frame_add_data(frame_t *frame, const uint8_t *data, size_t data_len)
{
    ESP_RETURN_ON_FALSE(frame && data && data_len, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    if (frame->data_len > frame->data_buffer_len || data_len > frame->data_buffer_len - frame->data_len) {
        ESP_LOGD(TAG, "Frame buffer overflow");
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(frame->data + frame->data_len, data, data_len);
    frame->data_len += data_len;
    return ESP_OK;
}

frame_t *frame_get_empty(void)
{
    frame_t *this_fb = NULL;
    if (empty_fb_queue && xQueueReceive(empty_fb_queue, &this_fb, 0) == pdPASS) {
        return this_fb;
    }
    return NULL;
}

frame_t *frame_get_filled(void)
{
    frame_t *this_fb = NULL;
    if (filled_fb_queue && xQueueReceive(filled_fb_queue, &this_fb, portMAX_DELAY) == pdPASS) {
        return this_fb;
    }
    return NULL;
}
