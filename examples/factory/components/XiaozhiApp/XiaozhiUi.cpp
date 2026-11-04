/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "XiaozhiUi.hpp"

#include <algorithm>
#include <cctype>
#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "cbin_font.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "font_awesome.h"
#include "font_emoji.h"

LV_FONT_DECLARE(font_puhui_basic_30_4);
LV_FONT_DECLARE(font_awesome_30_4);

namespace esp_brookesia::apps {

namespace {

constexpr char TAG[] = "XiaozhiUi";
constexpr char COMMON_FONT_PATH[] =
    BSP_SPIFFS_MOUNT_POINT "/xiaozhi/font.bin";
constexpr size_t MAX_COMMON_FONT_SIZE = 4 * 1024 * 1024;
constexpr int TOP_BAR_HEIGHT = 54;
constexpr int SUBTITLE_SINGLE_HEIGHT = 62;
constexpr int SUBTITLE_MULTILINE_HEIGHT = 132;
constexpr uint32_t COLOR_BACKGROUND = 0x070B18;
constexpr uint32_t COLOR_SURFACE = 0x111A2B;
constexpr uint32_t COLOR_TEXT = 0xF4F7FF;
constexpr uint32_t COLOR_HINT = 0x9CA9C1;
constexpr uint32_t COLOR_READY = 0x4ED6A2;
constexpr uint32_t COLOR_LISTENING = 0x3FA9FF;
constexpr uint32_t COLOR_SPEAKING = 0xB47AFF;
constexpr uint32_t COLOR_BUSY = 0xF2B84B;
constexpr uint32_t COLOR_ERROR = 0xF06A6A;

struct EmotionGlyph {
    const char *name;
    const char *utf8;
};

constexpr EmotionGlyph EMOTION_GLYPHS[] = {
    {"neutral", "\xF0\x9F\x98\xB6"},
    {"happy", "\xF0\x9F\x99\x82"},
    {"laughing", "\xF0\x9F\x98\x86"},
    {"funny", "\xF0\x9F\x98\x82"},
    {"sad", "\xF0\x9F\x98\x94"},
    {"angry", "\xF0\x9F\x98\xA0"},
    {"crying", "\xF0\x9F\x98\xAD"},
    {"loving", "\xF0\x9F\x98\x8D"},
    {"embarrassed", "\xF0\x9F\x98\xB3"},
    {"surprised", "\xF0\x9F\x98\xAF"},
    {"shocked", "\xF0\x9F\x98\xB1"},
    {"thinking", "\xF0\x9F\xA4\x94"},
    {"winking", "\xF0\x9F\x98\x89"},
    {"cool", "\xF0\x9F\x98\x8E"},
    {"relaxed", "\xF0\x9F\x98\x8C"},
    {"delicious", "\xF0\x9F\xA4\xA4"},
    {"kissy", "\xF0\x9F\x98\x98"},
    {"confident", "\xF0\x9F\x98\x8F"},
    {"sleepy", "\xF0\x9F\x98\xB4"},
    {"silly", "\xF0\x9F\x98\x9C"},
    {"confused", "\xF0\x9F\x99\x84"},
};

void prepareTransparentObject(lv_obj_t *object)
{
    lv_obj_remove_style_all(object);
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

void setLabelTextIfChanged(lv_obj_t *label, const char *text)
{
    if (!label) {
        return;
    }
    const char *value = text ? text : "";
    const char *current = lv_label_get_text(label);
    if (!current || strcmp(current, value) != 0) {
        lv_label_set_text(label, value);
    }
}

void setHidden(lv_obj_t *object, bool hidden)
{
    if (!object || lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) == hidden) {
        return;
    }

    if (hidden) {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

bool statusContains(const char *status, const char *word)
{
    if (!status || !word) {
        return false;
    }
    while (*status) {
        const char *cursor = status;
        const char *needle = word;
        while (*cursor && *needle && std::tolower(static_cast<unsigned char>(*cursor)) == *needle) {
            ++cursor;
            ++needle;
        }
        if (!*needle) {
            return true;
        }
        ++status;
    }
    return false;
}

uint32_t statusColor(const char *status)
{
    if (statusContains(status, "error") || statusContains(status, "network")) {
        return COLOR_ERROR;
    }
    if (statusContains(status, "listen")) {
        return COLOR_LISTENING;
    }
    if (statusContains(status, "speak")) {
        return COLOR_SPEAKING;
    }
    if (statusContains(status, "ready")) {
        return COLOR_READY;
    }
    if (statusContains(status, "connect") || statusContains(status, "prepar") || statusContains(status, "process")) {
        return COLOR_BUSY;
    }
    return COLOR_LISTENING;
}

} // namespace

XiaozhiUi::~XiaozhiUi()
{
    destroy();
    _emoji_font = nullptr;
    releaseTextFont();
    _text_font = &font_puhui_basic_30_4;
}

bool XiaozhiUi::preload()
{
    _text_font = &font_puhui_basic_30_4;
    if (_cbin_font) {
        _text_font = _cbin_font;
    } else {
        (void)loadTextFont();
    }
    if (!_emoji_font) {
        if (bsp_display_lock(-1)) {
            _emoji_font = font_emoji_64_init();
            bsp_display_unlock();
        } else {
            ESP_LOGW(TAG, "Unable to lock LVGL while loading the emoji font");
        }
    }
    return true;
}

bool XiaozhiUi::create(
    lv_obj_t *parent,
    ActionCallback action_callback,
    void *action_context,
    bool multiline_subtitle
)
{
    if (!parent) {
        return false;
    }

    destroyView();
    _action_callback = action_callback;
    _action_context = action_context;
    _text_font = _cbin_font ? _cbin_font : &font_puhui_basic_30_4;

    _root = lv_obj_create(parent);
    prepareTransparentObject(_root);
    lv_obj_set_size(_root, lv_pct(100), lv_pct(100));
    lv_obj_align(_root, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_root, lv_color_hex(COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(_root, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(_root, _text_font, LV_PART_MAIN);

    _top_bar = lv_obj_create(_root);
    prepareTransparentObject(_top_bar);
    lv_obj_set_size(_top_bar, lv_pct(100), TOP_BAR_HEIGHT);
    lv_obj_align(_top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(_top_bar, lv_color_hex(COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_top_bar, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_width(_top_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(_top_bar, lv_color_hex(0x26344D), LV_PART_MAIN);
    lv_obj_set_style_border_side(_top_bar, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);

    _network_label = lv_label_create(_top_bar);
    lv_obj_set_style_text_font(_network_label, &font_awesome_30_4, LV_PART_MAIN);
    lv_obj_set_style_text_color(_network_label, lv_color_hex(COLOR_HINT), LV_PART_MAIN);
    lv_obj_align(_network_label, LV_ALIGN_LEFT_MID, 16, 0);

    _status_label = lv_label_create(_top_bar);
    lv_obj_set_width(_status_label, lv_pct(74));
    lv_label_set_long_mode(_status_label, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(_status_label, _text_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(_status_label, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(_status_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_update_layout(_root);
    const int screen_short_side = std::min(lv_obj_get_width(_root), lv_obj_get_height(_root));
    const int orb_size = std::clamp(screen_short_side * 34 / 100, 156, 270);
    const int glow_size = std::clamp(orb_size * 150 / 100, 240, 390);
    const int ring_size = std::clamp(orb_size * 130 / 100, 205, 340);
    const int orb_y = -std::max(18, screen_short_side / 20);

    _orb_glow = lv_obj_create(_root);
    prepareTransparentObject(_orb_glow);
    lv_obj_set_size(_orb_glow, glow_size, glow_size);
    lv_obj_set_style_radius(_orb_glow, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_orb_glow, lv_color_hex(COLOR_LISTENING), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_orb_glow, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(_orb_glow, lv_color_hex(COLOR_LISTENING), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_orb_glow, 42, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(_orb_glow, LV_OPA_30, LV_PART_MAIN);
    lv_obj_align(_orb_glow, LV_ALIGN_CENTER, 0, orb_y);

    _orb_ring = lv_arc_create(_root);
    lv_obj_set_size(_orb_ring, ring_size, ring_size);
    lv_arc_set_range(_orb_ring, 0, 100);
    lv_arc_set_value(_orb_ring, 72);
    lv_arc_set_rotation(_orb_ring, 135);
    lv_obj_remove_flag(_orb_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(_orb_ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_arc_width(_orb_ring, std::max(5, orb_size / 28), LV_PART_MAIN);
    lv_obj_set_style_arc_width(_orb_ring, std::max(5, orb_size / 28), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_orb_ring, lv_color_hex(0x26344D), LV_PART_MAIN);
    lv_obj_set_style_arc_color(_orb_ring, lv_color_hex(COLOR_LISTENING), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(_orb_ring, true, LV_PART_INDICATOR);
    lv_obj_align(_orb_ring, LV_ALIGN_CENTER, 0, orb_y);

    _emoji_box = lv_obj_create(_root);
    prepareTransparentObject(_emoji_box);
    lv_obj_set_size(_emoji_box, orb_size, orb_size);
    lv_obj_set_style_radius(_emoji_box, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_emoji_box, lv_color_hex(0x182640), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_emoji_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_emoji_box, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_emoji_box, lv_color_hex(COLOR_LISTENING), LV_PART_MAIN);
    lv_obj_set_style_shadow_color(_emoji_box, lv_color_hex(COLOR_LISTENING), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_emoji_box, 18, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(_emoji_box, LV_OPA_30, LV_PART_MAIN);
    lv_obj_align(_emoji_box, LV_ALIGN_CENTER, 0, orb_y);
    lv_obj_add_flag(_emoji_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_emoji_box, actionEventCallback, LV_EVENT_CLICKED, this);

    _emoji_label = lv_label_create(_emoji_box);
    lv_obj_set_style_text_color(_emoji_label, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);
    lv_obj_center(_emoji_label);

    _activation_box = lv_obj_create(_root);
    prepareTransparentObject(_activation_box);
    lv_obj_set_size(_activation_box, lv_pct(78), std::max(250, screen_short_side * 40 / 100));
    lv_obj_set_flex_flow(_activation_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        _activation_box,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );
    lv_obj_set_style_pad_row(_activation_box, 18, LV_PART_MAIN);
    lv_obj_set_style_radius(_activation_box, 24, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_activation_box, lv_color_hex(COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_activation_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_activation_box, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(_activation_box, lv_color_hex(0x355175), LV_PART_MAIN);
    lv_obj_align(_activation_box, LV_ALIGN_CENTER, 0, orb_y);
    lv_obj_add_flag(_activation_box, LV_OBJ_FLAG_HIDDEN);

    _activation_code_label = lv_label_create(_activation_box);
    lv_obj_set_width(_activation_code_label, lv_pct(100));
    lv_obj_set_style_text_align(_activation_code_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(_activation_code_label, _text_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(_activation_code_label, lv_color_hex(COLOR_LISTENING), LV_PART_MAIN);

    _activation_message_label = lv_label_create(_activation_box);
    lv_obj_set_width(_activation_message_label, lv_pct(100));
    lv_obj_set_height(_activation_message_label, 150);
    lv_label_set_long_mode(_activation_message_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(_activation_message_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(_activation_message_label, _text_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        _activation_message_label,
        lv_color_hex(COLOR_HINT),
        LV_PART_MAIN
    );

    _subtitle_bar = lv_obj_create(_root);
    prepareTransparentObject(_subtitle_bar);
    lv_obj_set_size(
        _subtitle_bar,
        lv_pct(100),
        multiline_subtitle ? SUBTITLE_MULTILINE_HEIGHT : SUBTITLE_SINGLE_HEIGHT
    );
    lv_obj_align(_subtitle_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(_subtitle_bar, lv_color_hex(COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_subtitle_bar, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_width(_subtitle_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(_subtitle_bar, lv_color_hex(0x26344D), LV_PART_MAIN);
    lv_obj_set_style_border_side(_subtitle_bar, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(_subtitle_bar, 28, LV_PART_MAIN);
    lv_obj_add_flag(_subtitle_bar, LV_OBJ_FLAG_HIDDEN);

    _subtitle_role_label = lv_label_create(_subtitle_bar);
    lv_obj_set_width(_subtitle_role_label, 40);
    lv_obj_set_style_text_font(_subtitle_role_label, &font_awesome_30_4, LV_PART_MAIN);
    lv_obj_set_style_text_color(_subtitle_role_label, lv_color_hex(COLOR_LISTENING), LV_PART_MAIN);
    lv_obj_set_style_text_align(_subtitle_role_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(_subtitle_role_label, LV_ALIGN_LEFT_MID, 0, 0);

    _subtitle_label = lv_label_create(_subtitle_bar);
    lv_obj_set_width(_subtitle_label, std::max<lv_coord_t>(1, lv_obj_get_content_width(_subtitle_bar) - 48));
    if (multiline_subtitle) {
        lv_obj_set_height(_subtitle_label, SUBTITLE_MULTILINE_HEIGHT - 34);
    }
    lv_label_set_long_mode(
        _subtitle_label,
        multiline_subtitle ? LV_LABEL_LONG_MODE_WRAP : LV_LABEL_LONG_MODE_SCROLL_CIRCULAR
    );
    lv_obj_set_style_text_align(_subtitle_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(_subtitle_label, _text_font, LV_PART_MAIN);
    lv_obj_align(_subtitle_label, LV_ALIGN_BOTTOM_RIGHT, 0, -8);

    setNetworkReady(false);
    setStatus("");
    setEmotion("neutral");
    clearChatMessages();
    return true;
}

void XiaozhiUi::destroy()
{
    destroyView();
}

void XiaozhiUi::destroyView()
{
    if (_root) {
        lv_obj_delete(_root);
    }

    _root = nullptr;
    _top_bar = nullptr;
    _network_label = nullptr;
    _status_label = nullptr;
    _orb_glow = nullptr;
    _orb_ring = nullptr;
    _emoji_box = nullptr;
    _emoji_label = nullptr;
    _activation_box = nullptr;
    _activation_code_label = nullptr;
    _activation_message_label = nullptr;
    _subtitle_bar = nullptr;
    _subtitle_role_label = nullptr;
    _subtitle_label = nullptr;
    _network_state_valid = false;
    _network_ready = false;
    _activation_state_valid = false;
    _activation_visible = false;
    _action_callback = nullptr;
    _action_context = nullptr;
    _emotion_name[0] = '\0';
}

void XiaozhiUi::setNetworkReady(bool ready)
{
    if (!_network_label) {
        return;
    }
    if (_network_state_valid && _network_ready == ready) {
        return;
    }
    lv_label_set_text(
        _network_label,
        ready ? FONT_AWESOME_WIFI : FONT_AWESOME_WIFI_SLASH
    );
    lv_obj_set_style_text_color(
        _network_label,
        lv_color_hex(ready ? COLOR_READY : COLOR_ERROR),
        LV_PART_MAIN
    );
    _network_ready = ready;
    _network_state_valid = true;
}

void XiaozhiUi::setStatus(const char *status)
{
    setLabelTextIfChanged(_status_label, status);
    applyStatusStyle(status);
}

void XiaozhiUi::applyStatusStyle(const char *status)
{
    const uint32_t color = statusColor(status);
    if (_status_label) {
        lv_obj_set_style_text_color(_status_label, lv_color_hex(color), LV_PART_MAIN);
    }
    if (_emoji_box) {
        lv_obj_set_style_border_color(_emoji_box, lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_shadow_color(_emoji_box, lv_color_hex(color), LV_PART_MAIN);
    }
    if (_orb_glow) {
        lv_obj_set_style_bg_color(_orb_glow, lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_shadow_color(_orb_glow, lv_color_hex(color), LV_PART_MAIN);
    }
    if (_orb_ring) {
        lv_obj_set_style_arc_color(_orb_ring, lv_color_hex(color), LV_PART_INDICATOR);
    }
}

void XiaozhiUi::setChatMessage(const char *role, const char *text)
{
    if (!_subtitle_label || !_subtitle_bar) {
        return;
    }

    const char *value = text ? text : "";
    setLabelTextIfChanged(_subtitle_label, value);
    const bool assistant = role && strcmp(role, "assistant") == 0;
    if (_subtitle_role_label) {
        lv_label_set_text(_subtitle_role_label, value[0] ?
                          (assistant ? FONT_AWESOME_COMMENT : FONT_AWESOME_MICROPHONE) : "");
        lv_obj_set_style_text_color(_subtitle_role_label,
                                    lv_color_hex(assistant ? COLOR_SPEAKING : COLOR_LISTENING), LV_PART_MAIN);
    }
    setHidden(_subtitle_bar, value[0] == '\0');
}

void XiaozhiUi::clearChatMessages()
{
    setChatMessage("system", "");
}

void XiaozhiUi::setEmotion(const char *emotion)
{
    if (!_emoji_label) {
        return;
    }

    const char *name = emotion && emotion[0] ? emotion : "neutral";
    if (strcmp(_emotion_name, name) == 0) {
        return;
    }

    const char *emoji = emojiForEmotion(name);
    if (_emoji_font && emoji) {
        lv_obj_set_style_text_font(_emoji_label, _emoji_font, LV_PART_MAIN);
        lv_label_set_text(_emoji_label, emoji);
        snprintf(_emotion_name, sizeof(_emotion_name), "%s", name);
        return;
    }

    const char *icon = font_awesome_get_utf8(name);
    lv_obj_set_style_text_font(_emoji_label, &font_awesome_30_4, LV_PART_MAIN);
    lv_label_set_text(_emoji_label, icon ? icon : FONT_AWESOME_MICROCHIP_AI);
    snprintf(_emotion_name, sizeof(_emotion_name), "%s", name);
}

void XiaozhiUi::setActivation(const char *code, const char *message, bool visible)
{
    if (!_activation_box || !_emoji_box) {
        return;
    }

    setLabelTextIfChanged(_activation_code_label, code);
    setLabelTextIfChanged(_activation_message_label, message);
    if (!_activation_state_valid || _activation_visible != visible) {
        setHidden(_emoji_box, visible);
        setHidden(_activation_box, !visible);
        _activation_visible = visible;
        _activation_state_valid = true;
    }
}

void XiaozhiUi::actionEventCallback(lv_event_t *event)
{
    auto *ui = static_cast<XiaozhiUi *>(lv_event_get_user_data(event));
    if (ui && !ui->_activation_visible && ui->_action_callback) {
        ui->_action_callback(ui->_action_context);
    }
}

bool XiaozhiUi::loadTextFont()
{
    if (_cbin_font) {
        _text_font = _cbin_font;
        return true;
    }
    FILE *file = fopen(COMMON_FONT_PATH, "rb");
    if (!file) {
        ESP_LOGW(TAG, "Common font is unavailable: %s", COMMON_FONT_PATH);
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    long file_size = ftell(file);
    if (file_size <= 0 || static_cast<size_t>(file_size) > MAX_COMMON_FONT_SIZE ||
            fseek(file, 0, SEEK_SET) != 0) {
        ESP_LOGW(TAG, "Common font has an invalid size: %ld", file_size);
        fclose(file);
        return false;
    }

    size_t size = static_cast<size_t>(file_size);
    auto *data = static_cast<uint8_t *>(
                     heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                 );
    if (!data) {
        ESP_LOGW(TAG, "No PSRAM for common font (%u bytes)", static_cast<unsigned>(size));
        fclose(file);
        return false;
    }

    size_t read_size = fread(data, 1, size, file);
    fclose(file);
    if (read_size != size) {
        ESP_LOGW(TAG, "Common font read was incomplete (%u/%u)",
                 static_cast<unsigned>(read_size), static_cast<unsigned>(size));
        heap_caps_free(data);
        return false;
    }

    if (!bsp_display_lock(-1)) {
        ESP_LOGW(TAG, "Unable to lock LVGL while decoding the common font");
        heap_caps_free(data);
        return false;
    }
    lv_font_t *font = cbin_font_create(data);
    bsp_display_unlock();

    if (!font) {
        ESP_LOGW(TAG, "Common font could not be decoded");
        heap_caps_free(data);
        return false;
    }

    _font_data = data;
    _cbin_font = font;
    _text_font = font;
    ESP_LOGI(TAG, "Loaded common font from PSRAM (%u bytes)", static_cast<unsigned>(size));
    return true;
}

void XiaozhiUi::releaseTextFont()
{
    if (_cbin_font && !bsp_display_lock(-1)) {
        ESP_LOGE(TAG, "Unable to lock LVGL while releasing the common font");
        return;
    }
    if (_cbin_font) {
        cbin_font_delete(_cbin_font);
        _cbin_font = nullptr;
        bsp_display_unlock();
    }
    if (_font_data) {
        heap_caps_free(_font_data);
        _font_data = nullptr;
    }
}

const char *XiaozhiUi::emojiForEmotion(const char *emotion)
{
    if (!emotion) {
        return nullptr;
    }
    for (const auto &entry : EMOTION_GLYPHS) {
        if (strcmp(entry.name, emotion) == 0) {
            return entry.utf8;
        }
    }
    return nullptr;
}

} // namespace esp_brookesia::apps
