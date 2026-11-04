/**
 * @file lv_demo_music.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_demo_music.h"

#if APP_DEMO_MUSIC_ENABLE

#include "lv_demo_music_main.h"
#include "lv_demo_music_list.h"
#include <stdio.h>
#include <string.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
#if APP_DEMO_MUSIC_AUTO_PLAY
static void auto_step_cb(lv_timer_t *timer);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t *ctrl;
static lv_obj_t *list;
static lv_obj_t *music_parent;

#if APP_DEMO_MUSIC_AUTO_PLAY
static lv_timer_t *auto_step_timer;
#endif

static lv_color_t original_screen_bg_color;
static bool music_active;


static uint32_t active_track_cnt;
static file_iterator_instance_t *_file_iterator = NULL;

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

/* Returns zero when the format has no inexpensive duration metadata, including MP3. */
static uint32_t wav_duration_seconds(uint32_t track_id)
{
    if (!_file_iterator || track_id >= active_track_cnt) {
        return 0;
    }

    char path[256];
    if (file_iterator_get_full_path_from_index(_file_iterator, track_id, path, sizeof(path)) <= 0) {
        return 0;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        return 0;
    }

    uint8_t header[12];
    uint32_t byte_rate = 0;
    uint32_t data_size = 0;
    bool found_data = false;
    if (fread(header, 1, sizeof(header), file) == sizeof(header) &&
            memcmp(header, "RIFF", 4) == 0 && memcmp(header + 8, "WAVE", 4) == 0) {
        while (!found_data && fread(header, 1, 8, file) == 8) {
            const uint32_t chunk_size = read_le32(header + 4);
            if (memcmp(header, "fmt ", 4) == 0 && chunk_size >= 16) {
                uint8_t format[16];
                if (fread(format, 1, sizeof(format), file) != sizeof(format)) {
                    break;
                }
                byte_rate = read_le32(format + 8);
                if (chunk_size > sizeof(format)) {
                    fseek(file, chunk_size - sizeof(format), SEEK_CUR);
                }
            } else if (memcmp(header, "data", 4) == 0) {
                data_size = chunk_size;
                found_data = true;
            } else {
                fseek(file, chunk_size, SEEK_CUR);
            }
            if ((chunk_size & 1U) != 0U) {
                fseek(file, 1, SEEK_CUR);
            }
        }
    }
    fclose(file);
    return byte_rate == 0 ? 0 : data_size / byte_rate;
}

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_demo_music(lv_obj_t *parent, file_iterator_instance_t *file_iterator)
{
    /* A recreated Brookesia app can call us before the old screen is destroyed. */
    if (music_active) {
        lv_demo_music_close();
    }
    music_parent = parent;
    _file_iterator = file_iterator;

    active_track_cnt = (_file_iterator != NULL) ? file_iterator_get_count(_file_iterator) : 0;
    if (active_track_cnt > ACTIVE_TRACK_CNT)
    {
        active_track_cnt = ACTIVE_TRACK_CNT;
    }
    original_screen_bg_color = lv_obj_get_style_bg_color(music_parent, LV_PART_MAIN);
    lv_obj_set_style_bg_color(music_parent, lv_color_hex(0x343247), LV_PART_MAIN);

    list = lv_demo_music_list_create(parent);
    ctrl = lv_demo_music_main_create(parent, file_iterator);
    music_active = true;

    /* This factory player is always user-started. */
}

void lv_demo_music_close(void)
{
    if (!music_active) {
        return;
    }

#if APP_DEMO_MUSIC_AUTO_PLAY
    if (auto_step_timer) {
        lv_timer_delete(auto_step_timer);
        auto_step_timer = NULL;
    }
#endif
    lv_demo_music_main_close();

    /* List styles are released by its LV_EVENT_DELETE callback. */
    lv_obj_clean(music_parent);
    lv_demo_music_list_close();

    lv_obj_set_style_bg_color(music_parent, original_screen_bg_color, LV_PART_MAIN);
    ctrl = NULL;
    list = NULL;
    _file_iterator = NULL;
    music_parent = NULL;
    active_track_cnt = 0;
    music_active = false;
}

uint32_t lv_demo_music_get_track_count(void)
{
    return active_track_cnt;
}

const char *lv_demo_music_get_title(uint32_t track_id)
{
    if (_file_iterator == NULL)
    {
        return NULL;
    }

    if (track_id < active_track_cnt)
    {
        return file_iterator_get_name_from_index(_file_iterator, track_id);
    }
    return "";
}

const char *lv_demo_music_get_artist(uint32_t track_id)
{
    LV_UNUSED(track_id);
    return "";
}

const char *lv_demo_music_get_genre(uint32_t track_id)
{
    LV_UNUSED(track_id);
    return "";
}

uint32_t lv_demo_music_get_track_length(uint32_t track_id)
{
    return wav_duration_seconds(track_id);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#if APP_DEMO_MUSIC_AUTO_PLAY
static void auto_step_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    static uint32_t state = 0;

#if APP_DEMO_MUSIC_LARGE
    const lv_font_t *font_small = &lv_font_montserrat_22;
    const lv_font_t *font_large = &lv_font_montserrat_32;
#else
    const lv_font_t *font_small = &lv_font_montserrat_12;
    const lv_font_t *font_large = &lv_font_montserrat_16;
#endif

    switch (state)
    {
    case 5:
        lv_demo_music_album_next(true);
        break;

    case 6:
        lv_demo_music_album_next(true);
        break;
    case 7:
        lv_demo_music_album_next(true);
        break;
    case 8:
        lv_demo_music_play(0);
        break;
#if APP_DEMO_MUSIC_SQUARE || APP_DEMO_MUSIC_ROUND
    case 11:
        lv_obj_scroll_by(ctrl, 0, -LV_VER_RES, LV_ANIM_ON);
        break;
    case 13:
        lv_obj_scroll_by(ctrl, 0, -LV_VER_RES, LV_ANIM_ON);
        break;
#else
    case 12:
        lv_obj_scroll_by(ctrl, 0, -LV_VER_RES, LV_ANIM_ON);
        break;
#endif
    case 15:
        lv_obj_scroll_by(list, 0, -300, LV_ANIM_ON);
        break;
    case 16:
        lv_obj_scroll_by(list, 0, 300, LV_ANIM_ON);
        break;
    case 18:
        lv_demo_music_play(1);
        break;
    case 19:
        lv_obj_scroll_by(ctrl, 0, LV_VER_RES, LV_ANIM_ON);
        break;
#if APP_DEMO_MUSIC_SQUARE || APP_DEMO_MUSIC_ROUND
    case 20:
        lv_obj_scroll_by(ctrl, 0, LV_VER_RES, LV_ANIM_ON);
        break;
#endif
    case 30:
        lv_demo_music_play(2);
        break;
    case 40:
    {
        lv_obj_t *bg = lv_layer_top();
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x6f8af6), 0);
        lv_obj_set_style_text_color(bg, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
        lv_obj_fade_in(bg, 400, 0);
        lv_obj_t *dsc = lv_label_create(bg);
        lv_obj_set_style_text_font(dsc, font_small, 0);
        lv_label_set_text(dsc, "The average FPS is");
        lv_obj_align(dsc, LV_ALIGN_TOP_MID, 0, 90);

        lv_obj_t *num = lv_label_create(bg);
        lv_obj_set_style_text_font(num, font_large, 0);
#if LV_USE_PERF_MONITOR
        lv_display_t *disp = lv_display_get_default();
        const lv_sysmon_perf_info_t *info = lv_subject_get_pointer(&disp->perf_sysmon_backend.subject);
        lv_label_set_text_fmt(num, "%" LV_PRIu32, info->calculated.fps_avg_total);
#endif
        lv_obj_align(num, LV_ALIGN_TOP_MID, 0, 120);

        lv_obj_t *attr = lv_label_create(bg);
        lv_obj_set_style_text_align(attr, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(attr, font_small, 0);
#if APP_DEMO_MUSIC_SQUARE || APP_DEMO_MUSIC_ROUND
        lv_label_set_text(attr, "Copyright 2020 LVGL Kft.\nwww.lvgl.io | lvgl@lvgl.io");
#else
        lv_label_set_text(attr, "Copyright 2020 LVGL Kft. | www.lvgl.io | lvgl@lvgl.io");
#endif
        lv_obj_align(attr, LV_ALIGN_BOTTOM_MID, 0, -10);
        break;
    }
    case 41:
        lv_screen_load(lv_obj_create(NULL));
        lv_demo_music_pause();
        break;
    }
    state++;
}

#endif /*LV_DEMO_MUSIC_AUTO_PLAY*/

#endif /*LV_USE_DEMO_MUSIC*/
