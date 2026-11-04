/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "MusicPlayer.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:MusicPlayer"
#include "esp_lib_utils.h"
#include "sdkconfig.h"
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "gui_music/lv_demo_music.h"
#include "gui_music/lv_demo_music_main.h"
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>

#define MUSIC_DIR BSP_SPIFFS_MOUNT_POINT "/music"

LV_IMG_DECLARE(img_app_musicplayer);

static const char *TAG = "MusicPlayer";

namespace esp_brookesia::apps
{
    namespace {

    void deleteFileIterator(file_iterator_instance_t *iterator)
    {
        if (!iterator) {
            return;
        }
        for (size_t i = 0; i < iterator->count; ++i) {
            free(iterator->list[i]);
        }
        free(iterator->list);
        free(const_cast<char *>(iterator->directory_path));
        free(iterator);
    }

    file_iterator_instance_t *scanMusicFiles(const char *path)
    {
        // The managed iterator assumes opendir succeeds and also lists non-audio
        // entries. Build its public data structure only from playable files.
        DIR *directory = opendir(path);
        if (!directory) {
            return nullptr;
        }
        auto *iterator = static_cast<file_iterator_instance_t *>(calloc(1, sizeof(file_iterator_instance_t)));
        if (!iterator) {
            closedir(directory);
            return nullptr;
        }
        iterator->directory_path = strdup(path);
        if (!iterator->directory_path) {
            closedir(directory);
            deleteFileIterator(iterator);
            return nullptr;
        }
        while (const dirent *entry = readdir(directory)) {
            const char *extension = strrchr(entry->d_name, '.');
            if (entry->d_name[0] == '.' || !extension ||
                    (strcasecmp(extension, ".mp3") != 0 && strcasecmp(extension, ".wav") != 0)) {
                continue;
            }
            char full_path[256];
            const int length = snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            struct stat info = {};
            if (length < 0 || static_cast<size_t>(length) >= sizeof(full_path) ||
                    stat(full_path, &info) != 0 || !S_ISREG(info.st_mode)) {
                continue;
            }
            char *name = strdup(entry->d_name);
            auto **files = static_cast<char **>(realloc(iterator->list, (iterator->count + 1) * sizeof(char *)));
            if (!name || !files) {
                free(name);
                // realloc keeps the old list on failure; preserve a successful
                // allocation before the common cleanup path.
                if (files) iterator->list = files;
                closedir(directory);
                deleteFileIterator(iterator);
                return nullptr;
            }
            iterator->list = files;
            iterator->list[iterator->count++] = name;
        }
        closedir(directory);
        return iterator;
    }

    } // namespace

    MusicPlayer *MusicPlayer::_instance = nullptr;

    MusicPlayer *MusicPlayer::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new MusicPlayer(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    MusicPlayer::MusicPlayer(bool use_status_bar, bool use_navigation_bar) :
        App("MusicPlayer", &img_app_musicplayer, true, use_status_bar, use_navigation_bar)
    {
    }

    MusicPlayer::~MusicPlayer()
    {
        close();
    }

    bool MusicPlayer::run(void)
    {
        ESP_UTILS_LOGD("Run");

        _file_iterator = scanMusicFiles(MUSIC_DIR);
        if (!_file_iterator || file_iterator_get_count(_file_iterator) == 0) {
            releaseFileIterator();
            showStatus("No music found", "This firmware has no music tracks. Return to the launcher to use another app.");
            return true;
        }

        const esp_err_t result = bsp_extra_player_init();
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Audio player initialization failed: %s", esp_err_to_name(result));
            (void)bsp_extra_player_del();
            releaseFileIterator();
            showStatus("Music player unavailable", "Audio could not start. Return to the launcher and try again.");
            return true;
        }
        _player_ready = true;

        lv_demo_music(lv_scr_act(), _file_iterator);
        _demo_active = true;
        return true;
    }

    void MusicPlayer::showStatus(const char *title, const char *hint)
    {
        lv_obj_t *screen = lv_scr_act();
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x0B0B12), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
        _status_view = lv_obj_create(screen);
        lv_obj_remove_style_all(_status_view);
        lv_obj_set_size(_status_view, lv_pct(80), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(_status_view, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(_status_view, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(_status_view, 20, LV_PART_MAIN);
        lv_obj_remove_flag(_status_view, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_center(_status_view);

        lv_obj_t *icon = lv_label_create(_status_view);
        lv_label_set_text(icon, LV_SYMBOL_AUDIO);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_44, LV_PART_MAIN);
        lv_obj_set_style_text_color(icon, lv_color_hex(0x4F8BFF), LV_PART_MAIN);

        lv_obj_t *heading = lv_label_create(_status_view);
        lv_label_set_text(heading, title);
        lv_obj_set_style_text_font(heading, &lv_font_montserrat_32, LV_PART_MAIN);
        lv_obj_set_style_text_color(heading, lv_color_hex(0xF4F6FB), LV_PART_MAIN);

        lv_obj_t *message = lv_label_create(_status_view);
        lv_obj_set_width(message, lv_pct(100));
        lv_label_set_long_mode(message, LV_LABEL_LONG_MODE_WRAP);
        lv_label_set_text(message, hint);
        lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(message, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(message, lv_color_hex(0xA0A3B1), LV_PART_MAIN);

        lv_obj_t *back = lv_button_create(_status_view);
        lv_obj_set_size(back, 240, 60);
        lv_obj_set_style_bg_color(back, lv_color_hex(0x4F8BFF), LV_PART_MAIN);
        lv_obj_set_style_radius(back, 14, LV_PART_MAIN);
        lv_obj_add_event_cb(back, [](lv_event_t *event) {
            auto *app = static_cast<MusicPlayer *>(lv_event_get_user_data(event));
            if (app) app->notifyCoreClosed();
        }, LV_EVENT_CLICKED, this);
        lv_obj_t *back_label = lv_label_create(back);
        lv_label_set_text(back_label, "Back to launcher");
        lv_obj_set_style_text_font(back_label, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_center(back_label);
    }

    void MusicPlayer::releaseFileIterator()
    {
        deleteFileIterator(_file_iterator);
        _file_iterator = nullptr;
    }

    bool MusicPlayer::back(void)
    {
        ESP_UTILS_LOGD("Back");
        // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
        ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
        return true;
    }

    bool MusicPlayer::close(void)
    {
        ESP_UTILS_LOGD("Close");
        if (_demo_active) {
            lv_demo_music_close();
            _demo_active = false;
        }
        if (_player_ready) {
            const esp_err_t result = bsp_extra_player_del();
            if (result != ESP_OK) {
                ESP_LOGE(TAG, "Audio player cleanup failed: %s", esp_err_to_name(result));
            }
            _player_ready = false;
        }
        releaseFileIterator();
        if (_status_view) {
            lv_obj_delete(_status_view);
            _status_view = nullptr;
        }
        return true;
    }

    bool MusicPlayer::init()
    {
        ESP_UTILS_LOGD("Init");
        return true;
    }

    bool MusicPlayer::deinit()
    {
        ESP_UTILS_LOGD("Deinit");
        return close();
    }

    bool MusicPlayer::pause()
    {
        ESP_UTILS_LOGD("Pause");
        if (_demo_active) {
            lv_demo_music_exit_pause();
        }
        return true;
    }

    bool MusicPlayer::resume()
    {
        ESP_UTILS_LOGD("Resume");
        return true;
    }

} // namespace esp_brookesia::apps
