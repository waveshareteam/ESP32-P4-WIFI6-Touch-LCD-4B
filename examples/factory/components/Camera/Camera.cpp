/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "Camera.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"

#include "bsp/touch.h"
#include "esp_lib_utils.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "linux/videodev2.h"

#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Camera"

LV_IMG_DECLARE(img_app_camera);

#define ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB565
#define CAMERA_VIDEO_FMT V4L2_PIX_FMT_RGB565
#define CAMERA_PPA_COLOR_MODE PPA_SRM_COLOR_MODE_RGB565
#define CAMERA_BYTES_PER_PIXEL 2
#elif CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
#define CAMERA_VIDEO_FMT V4L2_PIX_FMT_RGB24
#define CAMERA_PPA_COLOR_MODE PPA_SRM_COLOR_MODE_RGB888
#define CAMERA_BYTES_PER_PIXEL 3
#else
#error "Unsupported BSP LCD color format"
#endif


namespace esp_brookesia::apps
{
    struct CoverCropConfig {
        uint32_t offset_x;
        uint32_t offset_y;
        uint32_t width;
        uint32_t height;
        float scale_x;
        float scale_y;
    };

    static CoverCropConfig computeCoverCrop(uint32_t src_w, uint32_t src_h, uint32_t dst_w, uint32_t dst_h)
    {
        CoverCropConfig crop = {
            .offset_x = 0,
            .offset_y = 0,
            .width = src_w,
            .height = src_h,
            .scale_x = 1.0f,
            .scale_y = 1.0f,
        };

        if (src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) {
            crop.width = crop.width ? crop.width : 1;
            crop.height = crop.height ? crop.height : 1;
            return crop;
        }

        if (static_cast<uint64_t>(src_w) * dst_h > static_cast<uint64_t>(src_h) * dst_w) {
            crop.width = static_cast<uint32_t>((static_cast<uint64_t>(src_h) * dst_w) / dst_h);
            crop.height = src_h;
        } else {
            crop.width = src_w;
            crop.height = static_cast<uint32_t>((static_cast<uint64_t>(src_w) * dst_h) / dst_w);
        }

        crop.width = crop.width ? crop.width : 1;
        crop.height = crop.height ? crop.height : 1;
        crop.offset_x = (src_w - crop.width) / 2;
        crop.offset_y = (src_h - crop.height) / 2;
        crop.scale_x = static_cast<float>(dst_w) / static_cast<float>(crop.width);
        crop.scale_y = static_cast<float>(dst_h) / static_cast<float>(crop.height);

        return crop;
    }

    Camera *Camera::_instance = nullptr;

    Camera *Camera::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new Camera(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    Camera::Camera(bool use_status_bar, bool use_navigation_bar) : App("Camera", &img_app_camera, true, use_status_bar, use_navigation_bar)
    {
    }

    Camera::~Camera()
    {
        close();
    }

    bool Camera::run(void)
    {
        ESP_UTILS_LOGD("Run");

        bsp_display_lock(-1);
        lv_obj_clean(lv_scr_act());
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
        _status_label = lv_label_create(lv_scr_act());
        lv_label_set_text(_status_label, "Camera");
        lv_obj_set_width(_status_label, BSP_LCD_H_RES);
        lv_obj_set_style_text_align(_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_30, LV_PART_MAIN);
        lv_obj_set_style_text_color(_status_label, lv_color_white(), LV_PART_MAIN);
        lv_obj_align(_status_label, LV_ALIGN_CENTER, 0, 0);
        bsp_display_unlock();

        if (!startPreview()) {
            bsp_display_lock(-1);
            if (_status_label) {
                lv_label_set_text(_status_label, "camera error");
            }
            bsp_display_unlock();
        }

        return true;
    }

    bool Camera::back(void)
    {
        ESP_UTILS_LOGD("Back");
        requestStopPreview();
        ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
        return true;
    }

    bool Camera::close()
    {
        ESP_UTILS_LOGD("Close");
        requestStopPreview();

        bsp_display_lock(-1);
        if (_status_label) {
            lv_obj_del(_status_label);
            _status_label = nullptr;
        }
        bsp_display_unlock();
        return true;
    }

    bool Camera::init()
    {
        ESP_UTILS_LOGD("Init");
        return true;
    }

    bool Camera::deinit()
    {
        ESP_UTILS_LOGD("Deinit");
        return true;
    }

    bool Camera::pause()
    {
        ESP_UTILS_LOGD("Pause");
        requestStopPreview();
        return true;
    }

    bool Camera::resume()
    {
        ESP_UTILS_LOGD("Resume");
        return true;
    }

    bool Camera::startPreview()
    {
        if (_preview_task_handle) {
            return true;
        }

        _preview_running = true;
        BaseType_t ret = xTaskCreatePinnedToCore(
            previewTaskEntry,
            "camera_preview",
            PREVIEW_TASK_STACK_SIZE,
            this,
            PREVIEW_TASK_PRIORITY,
            &_preview_task_handle,
            0);

        if (ret != pdPASS) {
            _preview_running = false;
            _preview_task_handle = nullptr;
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Create camera preview task failed");
            return false;
        }

        return true;
    }

    void Camera::requestStopPreview()
    {
        if (!_preview_task_handle) {
            stopDummyPreview();
            releaseCameraBuffers();
            return;
        }

        if (xTaskGetCurrentTaskHandle() == _preview_task_handle) {
            _preview_running = false;
            return;
        }

        _preview_running = false;
        _close_wait_task = xTaskGetCurrentTaskHandle();
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1500));
        _close_wait_task = nullptr;
    }

    esp_err_t Camera::initVideoDriver()
    {
        if (_video_driver_initialized) {
            return ESP_OK;
        }

        esp_video_init_csi_config_t csi_config[] = {
            {
                .sccb_config = {
                    .init_sccb = false,
                    .i2c_handle = bsp_i2c_get_handle(),
                    .freq = CONFIG_BSP_I2C_CLK_SPEED_HZ,
                },
                .reset_pin = GPIO_NUM_NC,
                .pwdn_pin = GPIO_NUM_NC,
            },
        };

        esp_video_init_config_t cam_config = {
            .csi = csi_config,
        };

        esp_err_t ret = esp_video_init(&cam_config);
        if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
            _video_driver_initialized = true;
            return ESP_OK;
        }

        ESP_LOGE(ESP_UTILS_LOG_TAG, "esp_video_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_err_t Camera::openVideoDevice()
    {
        if (_video_fd >= 0) {
            return ESP_OK;
        }

        _video_fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
        if (_video_fd < 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Open %s failed", ESP_VIDEO_MIPI_CSI_DEVICE_NAME);
            return ESP_FAIL;
        }

        struct v4l2_capability capability = {};
        if (ioctl(_video_fd, VIDIOC_QUERYCAP, &capability) != 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_QUERYCAP failed");
            ::close(_video_fd);
            _video_fd = -1;
            return ESP_FAIL;
        }

        struct v4l2_format format = {};
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(_video_fd, VIDIOC_G_FMT, &format) != 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_G_FMT failed");
            ::close(_video_fd);
            _video_fd = -1;
            return ESP_FAIL;
        }
        if (format.fmt.pix.width == 0 || format.fmt.pix.height == 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Camera returned an invalid capture size");
            ::close(_video_fd);
            _video_fd = -1;
            return ESP_ERR_INVALID_SIZE;
        }

        // The CSI node is initialized from the OV5647 sensor mode and rejects
        // width/height changes through VIDIOC_S_FMT. Keep that native size and
        // only request the LCD color format when the default differs.
        if (format.fmt.pix.pixelformat != CAMERA_VIDEO_FMT) {
            struct v4l2_format requested_format = format;
            requested_format.fmt.pix.pixelformat = CAMERA_VIDEO_FMT;
            if (ioctl(_video_fd, VIDIOC_S_FMT, &requested_format) != 0) {
                ESP_LOGE(
                    ESP_UTILS_LOG_TAG,
                    "VIDIOC_S_FMT color conversion failed for %" PRIu32 "x%" PRIu32,
                    format.fmt.pix.width,
                    format.fmt.pix.height
                );
                ::close(_video_fd);
                _video_fd = -1;
                return ESP_FAIL;
            }

            memset(&format, 0, sizeof(format));
            format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (ioctl(_video_fd, VIDIOC_G_FMT, &format) != 0) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_G_FMT after color conversion failed");
                ::close(_video_fd);
                _video_fd = -1;
                return ESP_FAIL;
            }
        }

        if (format.fmt.pix.pixelformat != CAMERA_VIDEO_FMT) {
            ESP_LOGE(
                ESP_UTILS_LOG_TAG,
                "Unsupported camera pixel format: 0x%08" PRIx32,
                format.fmt.pix.pixelformat
            );
            ::close(_video_fd);
            _video_fd = -1;
            return ESP_ERR_NOT_SUPPORTED;
        }

        _camera_width = format.fmt.pix.width;
        _camera_height = format.fmt.pix.height;
        _camera_buffer_size = format.fmt.pix.sizeimage;
        if (_camera_buffer_size == 0 && format.fmt.pix.bytesperline != 0) {
            _camera_buffer_size = format.fmt.pix.bytesperline * _camera_height;
        }
        if (_camera_buffer_size == 0) {
            _camera_buffer_size = _camera_width * _camera_height * CAMERA_BYTES_PER_PIXEL;
        }

        ESP_LOGI(
            ESP_UTILS_LOG_TAG,
            "Camera format: %" PRIu32 "x%" PRIu32 ", buffer: %u bytes",
            _camera_width,
            _camera_height,
            static_cast<unsigned>(_camera_buffer_size)
        );

        return ESP_OK;
    }

    esp_err_t Camera::setupCameraBuffers()
    {
#ifdef CONFIG_CACHE_L2_CACHE_LINE_SIZE
        _data_cache_line_size = CONFIG_CACHE_L2_CACHE_LINE_SIZE;
#else
        _data_cache_line_size = 64;
#endif

        struct v4l2_requestbuffers req = {};
        req.count = CAMERA_BUFFER_COUNT;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_USERPTR;

        if (ioctl(_video_fd, VIDIOC_REQBUFS, &req) != 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_REQBUFS failed");
            return ESP_FAIL;
        }

        for (int i = 0; i < CAMERA_BUFFER_COUNT; i++) {
            struct v4l2_buffer buf = {};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;
            buf.index = i;

            if (ioctl(_video_fd, VIDIOC_QUERYBUF, &buf) != 0) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_QUERYBUF %d failed", i);
                return ESP_FAIL;
            }

            size_t alloc_size = buf.length ? buf.length : _camera_buffer_size;
            _camera_buffer_lengths[i] = alloc_size;
            _camera_buffers[i] = heap_caps_aligned_calloc(_data_cache_line_size, 1, alloc_size, MALLOC_CAP_SPIRAM);
            if (!_camera_buffers[i]) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "Camera buffer %d allocation failed", i);
                return ESP_ERR_NO_MEM;
            }

            buf.m.userptr = (unsigned long)_camera_buffers[i];
            buf.length = alloc_size;

            if (ioctl(_video_fd, VIDIOC_QBUF, &buf) != 0) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_QBUF %d failed", i);
                return ESP_FAIL;
            }
        }

        return ESP_OK;
    }

    esp_err_t Camera::startDummyPreview()
    {
        _display = lv_display_get_default();
        ESP_RETURN_ON_FALSE(_display != nullptr, ESP_ERR_INVALID_STATE, ESP_UTILS_LOG_TAG, "LVGL display is not ready");

        if (_ppa_srm_handle == nullptr) {
            ppa_client_config_t ppa_srm_config = {
                .oper_type = PPA_OPERATION_SRM,
            };
            ESP_RETURN_ON_ERROR(ppa_register_client(&ppa_srm_config, &_ppa_srm_handle), ESP_UTILS_LOG_TAG, "Register PPA SRM failed");
        }

        ESP_RETURN_ON_ERROR(bsp_touch_new(NULL, &_touch_handle), ESP_UTILS_LOG_TAG, "Create touch handle failed");

        ESP_RETURN_ON_ERROR(esp_lv_adapter_set_dummy_draw(_display, true), ESP_UTILS_LOG_TAG, "Enable dummy draw failed");
        _dummy_enabled = true;

        ESP_RETURN_ON_ERROR(esp_lv_adapter_pause(-1), ESP_UTILS_LOG_TAG, "Pause LVGL failed");
        _lvgl_paused = true;

        return ESP_OK;
    }

    void Camera::stopDummyPreview()
    {
        if (_lvgl_paused) {
            esp_lv_adapter_resume();
            _lvgl_paused = false;
        }

        if (_dummy_enabled && _display) {
            esp_lv_adapter_set_dummy_draw(_display, false);
            _dummy_enabled = false;
        }

        if (_touch_handle) {
            esp_lcd_touch_del(_touch_handle);
            _touch_handle = nullptr;
        }

        _touch_active = false;
    }

    void Camera::releaseCameraBuffers()
    {
        for (int i = 0; i < CAMERA_BUFFER_COUNT; i++) {
            if (_camera_buffers[i]) {
                heap_caps_free(_camera_buffers[i]);
                _camera_buffers[i] = nullptr;
            }
            _camera_buffer_lengths[i] = 0;
        }

        if (_video_fd >= 0) {
            ::close(_video_fd);
            _video_fd = -1;
        }
    }

    bool Camera::shouldExitBySwipe()
    {
        if (!_touch_handle) {
            return false;
        }

        esp_lcd_touch_read_data(_touch_handle);

        esp_lcd_touch_point_data_t points[1] = {};
        uint8_t point_num = 0;
        esp_err_t ret = esp_lcd_touch_get_data(_touch_handle, points, &point_num, 1);

        if (ret != ESP_OK || point_num == 0) {
            _touch_active = false;
            return false;
        }

        if (!_touch_active) {
            _touch_active = true;
            _touch_start_x = points[0].x;
            _touch_start_y = points[0].y;
            return false;
        }

        int dx = abs((int)points[0].x - (int)_touch_start_x);
        int dy = abs((int)points[0].y - (int)_touch_start_y);
        return dx > SWIPE_EXIT_THRESHOLD || dy > SWIPE_EXIT_THRESHOLD;
    }

    esp_err_t Camera::handleFrame()
    {
        void *lcd_buffer = esp_lv_adapter_dummy_draw_get_free_buf(_display);
        ESP_RETURN_ON_FALSE(lcd_buffer != nullptr, ESP_ERR_NOT_FOUND, ESP_UTILS_LOG_TAG, "No free LCD frame buffer");

        struct v4l2_buffer v4l2_buf = {};
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory = V4L2_MEMORY_USERPTR;

        if (ioctl(_video_fd, VIDIOC_DQBUF, &v4l2_buf) != 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_DQBUF failed");
            return ESP_FAIL;
        }

        const uint32_t display_w = BSP_LCD_H_RES;
        const uint32_t display_h = BSP_LCD_V_RES;
        CoverCropConfig crop = computeCoverCrop(_camera_width, _camera_height, display_w, display_h);

        ppa_srm_oper_config_t srm_config = {};
        srm_config.in.buffer = _camera_buffers[v4l2_buf.index];
        srm_config.in.pic_w = _camera_width;
        srm_config.in.pic_h = _camera_height;
        srm_config.in.block_w = crop.width;
        srm_config.in.block_h = crop.height;
        srm_config.in.block_offset_x = crop.offset_x;
        srm_config.in.block_offset_y = crop.offset_y;
        srm_config.in.srm_cm = CAMERA_PPA_COLOR_MODE;

        srm_config.out.buffer = lcd_buffer;
        srm_config.out.buffer_size = ALIGN_UP(display_w * display_h * CAMERA_BYTES_PER_PIXEL, _data_cache_line_size);
        srm_config.out.pic_w = display_w;
        srm_config.out.pic_h = display_h;
        srm_config.out.block_offset_x = 0;
        srm_config.out.block_offset_y = 0;
        srm_config.out.srm_cm = CAMERA_PPA_COLOR_MODE;

        srm_config.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
        srm_config.scale_x = crop.scale_x;
        srm_config.scale_y = crop.scale_y;
        srm_config.mirror_x = 0;
        srm_config.mirror_y = 0;
        srm_config.rgb_swap = 0;
        srm_config.byte_swap = 0;
        srm_config.mode = PPA_TRANS_MODE_BLOCKING;

        esp_err_t ret = ppa_do_scale_rotate_mirror(_ppa_srm_handle, &srm_config);
        if (ret == ESP_OK && _dummy_enabled) {
            ret = esp_lv_adapter_dummy_draw_flush_buf(_display, lcd_buffer);
        }

        v4l2_buf.m.userptr = (unsigned long)_camera_buffers[v4l2_buf.index];
        v4l2_buf.length = _camera_buffer_lengths[v4l2_buf.index];
        if (ioctl(_video_fd, VIDIOC_QBUF, &v4l2_buf) != 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_QBUF failed");
            return ESP_FAIL;
        }

        return ret;
    }

    void Camera::previewTask()
    {
        bool request_close = false;
        esp_err_t ret = initVideoDriver();
        if (ret == ESP_OK) {
            ret = openVideoDevice();
        }
        if (ret == ESP_OK) {
            ret = setupCameraBuffers();
        }
        if (ret == ESP_OK) {
            ret = startDummyPreview();
        }

        if (ret == ESP_OK) {
            int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (ioctl(_video_fd, VIDIOC_STREAMON, &type) != 0) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_STREAMON failed");
                ret = ESP_FAIL;
            }
        }

        while (_preview_running && ret == ESP_OK) {
            ret = handleFrame();
            if (shouldExitBySwipe()) {
                request_close = true;
                _preview_running = false;
            }
        }

        if (_video_fd >= 0) {
            int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ioctl(_video_fd, VIDIOC_STREAMOFF, &type);
        }

        stopDummyPreview();
        releaseCameraBuffers();

        _preview_running = false;
        _preview_task_handle = nullptr;

        if (_close_wait_task) {
            xTaskNotifyGive(_close_wait_task);
        }

        if (request_close) {
            notifyCoreClosed();
        }

        vTaskDelete(NULL);
    }

    void Camera::previewTaskEntry(void *arg)
    {
        static_cast<Camera *>(arg)->previewTask();
    }

} // namespace esp_brookesia::apps
