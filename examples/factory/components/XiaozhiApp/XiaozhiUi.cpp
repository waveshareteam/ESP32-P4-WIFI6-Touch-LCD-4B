/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "XiaozhiUi.hpp"

#include <initializer_list>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "cbin_font.h"
#include "misc/cache/instance/lv_image_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"

LV_FONT_DECLARE(font_puhui_basic_30_4);

namespace esp_brookesia::apps {

namespace {

constexpr uint32_t TICK_MS = 33;
constexpr char TAG[] = "XiaozhiUi";
constexpr char COMMON_FONT_PATH[] = BSP_SPIFFS_MOUNT_POINT "/xiaozhi/font.bin";
constexpr size_t MAX_COMMON_FONT_SIZE = 4 * 1024 * 1024;
// Preserve the reference orb UI geometry at the LCD-4B's 720-square scale.
constexpr int SCREEN = 720;
constexpr int ORB_Y = -46;
constexpr int ORB_SIZE = 241;
constexpr int GLOW_SIZE = 525;
constexpr int RING_SIZE = 463;
constexpr float RING_INNER = 148;
constexpr float RING_REACH = 46;
constexpr int EYE_W = 28;
constexpr int EYE_H = 56;
constexpr int EYE_GAP = 37;
constexpr int EYE_Y = -15;
constexpr int ARC_EYE_SIZE = 56;
constexpr int MOUTH_Y = 40;
constexpr int CHIP_Y = 68;
constexpr int USER_Y = 170;
constexpr int REPLY_Y = 235;
constexpr int TEXT_W = 457;
constexpr uint32_t BLINK_MS = 180;
constexpr uint32_t TOAST_MS = 1200;

constexpr uint32_t NAVY = 0x070B18;
constexpr uint32_t TEXT_CHIP = 0xC9D1DE;
constexpr uint32_t TEXT_DIM = 0x7C8598;
constexpr uint32_t TEXT_USER = 0x8E98AA;

struct Rgb {
    float r, g, b;
};

Rgb rgbOf(uint32_t hex)
{
    return {((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f};
}

Rgb mix(Rgb a, Rgb b, float t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

float clamp01(float v)
{
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}

uint32_t modeColor(XiaozhiUi::Mode mode)
{
    switch (mode) {
    case XiaozhiUi::Mode::Offline:
        return 0x6B7280;
    case XiaozhiUi::Mode::Busy:
    case XiaozhiUi::Mode::Activation:
        return 0x5B8CFF;
    case XiaozhiUi::Mode::Ready:
        return 0x4F7CFF;
    case XiaozhiUi::Mode::Listening:
        return 0x2EE6A6;
    case XiaozhiUi::Mode::Thinking:
        return 0xFFB84D;
    case XiaozhiUi::Mode::Speaking:
        return 0xA78BFA;
    case XiaozhiUi::Mode::Error:
    default:
        return 0xFF5C7A;
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

void copyText(char *destination, size_t capacity, const char *source)
{
    snprintf(destination, capacity, "%s", source ? source : "");
}

lv_obj_t *createPlain(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    return obj;
}

lv_obj_t *createArc(lv_obj_t *parent, int size, int width)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(arc, size, size);
    lv_arc_set_bg_angles(arc, 0, 0);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
    lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    return arc;
}

/* 0 to 1 and back over the blink */
float blinkClosure(uint32_t elapsed_ms)
{
    const float t = static_cast<float>(elapsed_ms) / BLINK_MS;
    return t < 0.5f ? t * 2 : (1 - t) * 2;
}

constexpr uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

} // namespace

XiaozhiUi::~XiaozhiUi()
{
    destroy();
}

bool XiaozhiUi::preload()
{
    if (!_cbin_font) {
        (void)loadTextFont();
    }
    _text_font = _cbin_font ? _cbin_font : &font_puhui_basic_30_4;
    _large_font = _text_font;
    return _text_font && _large_font;
}

bool XiaozhiUi::allocateImages()
{
    const struct {
        Image *image;
        int size;
        lv_color_format_t format;
        int bytes;
    } images[] = {
        {&_background_image, SCREEN, LV_COLOR_FORMAT_RGB565, 2},
        {&_orb_image, ORB_SIZE, LV_COLOR_FORMAT_ARGB8888, 4},
        {&_glow_image, GLOW_SIZE, LV_COLOR_FORMAT_ARGB8888, 4},
    };
    for (const auto &entry : images) {
        if (entry.image->pixels) {
            continue;
        }
        const size_t size = static_cast<size_t>(entry.size) * entry.size * entry.bytes;
        entry.image->pixels = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!entry.image->pixels) {
            return false;
        }
        lv_image_dsc_t &dsc = entry.image->dsc;
        dsc = {};
        dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        dsc.header.cf = entry.format;
        dsc.header.w = entry.size;
        dsc.header.h = entry.size;
        dsc.header.stride = entry.size * entry.bytes;
        dsc.data_size = size;
        dsc.data = entry.image->pixels;
    }
    return true;
}

void XiaozhiUi::releaseImages()
{
    for (Image *image : {&_background_image, &_orb_image, &_glow_image}) {
        if (image->pixels) {
            lv_image_cache_drop(&image->dsc);
            heap_caps_free(image->pixels);
            image->pixels = nullptr;
        }
    }
}

/* A dark wash of the mode's colour around the orb, dithered so it shows no bands in RGB565 */
void XiaozhiUi::renderBackground(uint32_t color)
{
    constexpr int LUT_SIZE = 525;
    static Rgb lut[LUT_SIZE];
    const Rgb navy = rgbOf(NAVY);
    const Rgb tint = rgbOf(color);
    for (int r = 0; r < LUT_SIZE; r++) {
        const float t = clamp01(r / 510.0f);
        const float fade = (1 - t) * (1 - t);
        const Rgb c = mix(navy, tint, 0.16f * (1 - t));
        lut[r] = {c.r * fade * 255, c.g * fade * 255, c.b * fade * 255};
    }
    auto *out = reinterpret_cast<uint16_t *>(_background_image.pixels);
    const float cy = SCREEN / 2.0f + ORB_Y;
    for (int y = 0; y < SCREEN; y++) {
        const float dy = y + 0.5f - cy;
        for (int x = 0; x < SCREEN; x++) {
            const float dx = x + 0.5f - SCREEN / 2.0f;
            int r = static_cast<int>(sqrtf(dx * dx + dy * dy));
            r = r < LUT_SIZE ? r : LUT_SIZE - 1;
            const float d = (BAYER[y & 3][x & 3] + 0.5f) / 16.0f;
            const int r5 = static_cast<int>(lut[r].r * 31 / 255 + d);
            const int g6 = static_cast<int>(lut[r].g * 63 / 255 + d);
            const int b5 = static_cast<int>(lut[r].b * 31 / 255 + d);
            out[y * SCREEN + x] = static_cast<uint16_t>((r5 > 31 ? 31 : r5) << 11 | (g6 > 63 ? 63 : g6) << 5 |
                                                        (b5 > 31 ? 31 : b5));
        }
    }
}

/* A glossy sphere lit from the upper left: shaded body, bright rim and a specular spot */
void XiaozhiUi::renderOrb(uint32_t color)
{
    const Rgb base = rgbOf(color);
    const Rgb dark = {base.r * 0.30f, base.g * 0.30f, base.b * 0.36f};
    const Rgb light = mix(base, {1, 1, 1}, 0.30f);
    const Rgb rim_color = mix(base, {1, 1, 1}, 0.55f);
    const float radius = ORB_SIZE / 2.0f;
    uint8_t *p = _orb_image.pixels;
    for (int y = 0; y < ORB_SIZE; y++) {
        for (int x = 0; x < ORB_SIZE; x++, p += 4) {
            const float nx = (x + 0.5f - radius) / radius;
            const float ny = (y + 0.5f - radius) / radius;
            const float d = sqrtf(nx * nx + ny * ny);
            const float alpha = clamp01((1 - d) * radius + 0.5f);
            if (alpha <= 0) {
                p[0] = p[1] = p[2] = p[3] = 0;
                continue;
            }
            const float nz = sqrtf(fmaxf(0, 1 - d * d));
            const float diffuse = fmaxf(0, -0.45f * nx - 0.55f * ny + 0.70f * nz);
            Rgb c = mix(dark, light, powf(diffuse, 1.2f));
            c = mix(c, rim_color, powf(1 - nz, 3.0f) * 0.55f);
            const float spec = powf(fmaxf(0, -0.244f * nx - 0.298f * ny + 0.922f * nz), 48.0f) * 0.85f;
            c = mix(c, {1, 1, 1}, spec);
            p[0] = static_cast<uint8_t>(clamp01(c.b) * 255);
            p[1] = static_cast<uint8_t>(clamp01(c.g) * 255);
            p[2] = static_cast<uint8_t>(clamp01(c.r) * 255);
            p[3] = static_cast<uint8_t>(alpha * 255);
        }
    }
}

/* A soft halo that fades out from the orb's edge */
void XiaozhiUi::renderGlow(uint32_t color)
{
    const Rgb c = mix(rgbOf(color), {1, 1, 1}, 0.15f);
    const uint8_t b = static_cast<uint8_t>(c.b * 255);
    const uint8_t g = static_cast<uint8_t>(c.g * 255);
    const uint8_t r = static_cast<uint8_t>(c.r * 255);
    const float centre = GLOW_SIZE / 2.0f;
    const float edge = ORB_SIZE / 2.0f - 4;
    uint8_t *p = _glow_image.pixels;
    for (int y = 0; y < GLOW_SIZE; y++) {
        for (int x = 0; x < GLOW_SIZE; x++, p += 4) {
            const float dx = x + 0.5f - centre;
            const float dy = y + 0.5f - centre;
            const float dist = sqrtf(dx * dx + dy * dy);
        const float t = fmaxf(0, dist - edge) / 71.0f;
            const float alpha = 0.55f * expf(-t * t) * clamp01((centre - dist) / 24.0f);
            p[0] = b;
            p[1] = g;
            p[2] = r;
            p[3] = static_cast<uint8_t>(alpha * 255);
        }
    }
}

bool XiaozhiUi::create(
    lv_obj_t *parent,
    ActionCallback action_callback,
    void *action_context,
    bool multiline_subtitle
)
{
    (void)multiline_subtitle;
    if (!parent) {
        return false;
    }
    destroyView();
    preload();
    if (!allocateImages()) {
        releaseImages();
        return false;
    }
    _action_callback = action_callback;
    _action_context = action_context;

    _root = lv_obj_create(parent);
    lv_obj_remove_style_all(_root);
    lv_obj_set_size(_root, lv_pct(100), lv_pct(100));
    lv_obj_center(_root);
    lv_obj_set_style_bg_color(_root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(_root, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(_root, _text_font, LV_PART_MAIN);
    lv_obj_remove_flag(_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_root, clickCallback, LV_EVENT_CLICKED, this);

    _background = lv_image_create(_root);
    lv_obj_remove_flag(_background, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(_background);

    _glow = lv_image_create(_root);
    lv_obj_remove_flag(_glow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(_glow, LV_ALIGN_CENTER, 0, ORB_Y);

    // The voice bars and the thinking dots are drawn straight into this object's area
    _ring = createPlain(_root);
    lv_obj_set_size(_ring, RING_SIZE, RING_SIZE);
    lv_obj_align(_ring, LV_ALIGN_CENTER, 0, ORB_Y);
    lv_obj_add_event_cb(_ring, ringDrawCallback, LV_EVENT_DRAW_MAIN, this);

    _orb = lv_image_create(_root);
    lv_obj_remove_flag(_orb, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(_orb, LV_ALIGN_CENTER, 0, ORB_Y);

    // The face lives in a plain box over the orb image
    lv_obj_t *face = createPlain(_root);
    lv_obj_set_size(face, ORB_SIZE, ORB_SIZE);
    lv_obj_align(face, LV_ALIGN_CENTER, 0, ORB_Y);
    for (int i = 0; i < 2; i++) {
        const int x = i == 0 ? -EYE_GAP : EYE_GAP;
        _eye_pills[i] = createPlain(face);
        lv_obj_set_size(_eye_pills[i], EYE_W, EYE_H);
        lv_obj_set_style_radius(_eye_pills[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(_eye_pills[i], lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_eye_pills[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(_eye_pills[i], 14, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(_eye_pills[i], lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(_eye_pills[i], LV_OPA_40, LV_PART_MAIN);
        lv_obj_align(_eye_pills[i], LV_ALIGN_CENTER, x, EYE_Y);

        _eye_arcs[i] = createArc(face, ARC_EYE_SIZE, 6);
        lv_obj_align(_eye_arcs[i], LV_ALIGN_CENTER, x, EYE_Y + 6);
    }
    _mouth = createPlain(face);
    lv_obj_set_style_radius(_mouth, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_mouth, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_mouth, LV_OPA_90, LV_PART_MAIN);
    lv_obj_align(_mouth, LV_ALIGN_CENTER, 0, MOUTH_Y);
    _mouth_arc = createArc(face, 34, 5);
    lv_obj_align(_mouth_arc, LV_ALIGN_CENTER, 0, MOUTH_Y - 10);

    _code_label = lv_label_create(face);
    lv_obj_set_style_text_font(_code_label, _large_font, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(_code_label, 3, LV_PART_MAIN);
    lv_obj_center(_code_label);
    lv_obj_add_flag(_code_label, LV_OBJ_FLAG_HIDDEN);

    // The state in a frosted pill with a dot of the mode's colour
    _chip = createPlain(_root);
    lv_obj_set_size(_chip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(_chip, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_chip, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_chip, 22, LV_PART_MAIN);
    lv_obj_set_style_border_color(_chip, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(_chip, 36, LV_PART_MAIN);
    lv_obj_set_style_border_width(_chip, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(_chip, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(_chip, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(_chip, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(_chip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_chip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(_chip, LV_ALIGN_TOP_MID, 0, CHIP_Y);
    _chip_dot = createPlain(_chip);
    lv_obj_set_size(_chip_dot, 8, 8);
    lv_obj_set_style_radius(_chip_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_chip_dot, LV_OPA_COVER, LV_PART_MAIN);
    _status_label = lv_label_create(_chip);
    lv_obj_set_style_max_width(_status_label, 386, LV_PART_MAIN);
    lv_label_set_long_mode(_status_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_color(_status_label, lv_color_hex(TEXT_CHIP), LV_PART_MAIN);

    // What was heard, then the reply; the reply hangs from the bottom of its box to show its end
    _user_label = lv_label_create(_root);
    lv_obj_set_width(_user_label, TEXT_W);
    lv_label_set_long_mode(_user_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(_user_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(_user_label, lv_color_hex(TEXT_USER), LV_PART_MAIN);
    lv_obj_set_style_max_height(_user_label, lv_font_get_line_height(_text_font), LV_PART_MAIN);
    lv_obj_align(_user_label, LV_ALIGN_CENTER, 0, USER_Y);
    _reply_box = createPlain(_root);
    lv_obj_set_size(_reply_box, TEXT_W, 2 * lv_font_get_line_height(_text_font) + 4);
    lv_obj_align(_reply_box, LV_ALIGN_CENTER, 0, REPLY_Y);
    _reply_label = lv_label_create(_reply_box);
    lv_obj_set_width(_reply_label, TEXT_W);
    lv_label_set_long_mode(_reply_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(_reply_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(_reply_label, 4, LV_PART_MAIN);
    lv_obj_align(_reply_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    _mode_valid = false;
    _face_valid = false;
    _blink_ms = 3000;
    setMode(Mode::Offline);
    setEmotion("neutral");
    updateSubtitle();
    _tick = lv_timer_create(tickCallback, TICK_MS, this);
    return true;
}

void XiaozhiUi::destroy()
{
    destroyView();
    releaseImages();
    releaseTextFont();
}

bool XiaozhiUi::loadTextFont()
{
    FILE *file = fopen(COMMON_FONT_PATH, "rb");
    if (!file) {
        ESP_LOGW(TAG, "Common font is unavailable: %s", COMMON_FONT_PATH);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    const long file_size = ftell(file);
    if (file_size <= 0 || static_cast<size_t>(file_size) > MAX_COMMON_FONT_SIZE ||
            fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    const size_t size = static_cast<size_t>(file_size);
    auto *data = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!data || fread(data, 1, size, file) != size) {
        fclose(file);
        heap_caps_free(data);
        return false;
    }
    fclose(file);
    if (!bsp_display_lock(-1)) {
        heap_caps_free(data);
        return false;
    }
    lv_font_t *font = cbin_font_create(data);
    bsp_display_unlock();
    if (!font) {
        heap_caps_free(data);
        return false;
    }
    _font_data = data;
    _cbin_font = font;
    return true;
}

void XiaozhiUi::releaseTextFont()
{
    if (_cbin_font && bsp_display_lock(-1)) {
        cbin_font_delete(_cbin_font);
        bsp_display_unlock();
    }
    _cbin_font = nullptr;
    heap_caps_free(_font_data);
    _font_data = nullptr;
}

void XiaozhiUi::destroyView()
{
    if (_tick) {
        lv_timer_delete(_tick);
        _tick = nullptr;
    }
    if (_root) {
        lv_obj_delete(_root);
    }
    _root = nullptr;
    _background = _glow = _ring = _orb = nullptr;
    _eye_pills[0] = _eye_pills[1] = nullptr;
    _eye_arcs[0] = _eye_arcs[1] = nullptr;
    _mouth = _mouth_arc = _code_label = nullptr;
    _chip = _chip_dot = _status_label = nullptr;
    _user_label = _reply_box = _reply_label = nullptr;
    _activation_visible = false;
    _toast_until = 0;
}

void XiaozhiUi::setNetworkReady(bool ready)
{
    // The status bar shows the network; offline shows as the grey, sleeping orb
    (void)ready;
}

void XiaozhiUi::setStatus(const char *status)
{
    copyText(_status, sizeof(_status), status);
    if (_toast_until == 0) {
        setLabelTextIfChanged(_status_label, _status);
    }
}

void XiaozhiUi::showToast(const char *text)
{
    if (!_status_label) {
        return;
    }
    setLabelTextIfChanged(_status_label, text);
    lv_obj_set_style_text_color(_status_label, lv_color_white(), LV_PART_MAIN);
    _toast_until = lv_tick_get() + TOAST_MS;
    if (_toast_until == 0) {
        _toast_until = 1;
    }
}

void XiaozhiUi::setMode(Mode mode)
{
    if (_mode_valid && _mode == mode) {
        return;
    }
    _mode = mode;
    _mode_valid = true;
    applyMode();
}

void XiaozhiUi::applyMode()
{
    if (!_root) {
        return;
    }
    const uint32_t color = modeColor(_mode);
    renderBackground(color);
    renderOrb(color);
    renderGlow(color);
    struct {
        lv_obj_t *obj;
        Image *image;
    } images[] = {{_background, &_background_image}, {_orb, &_orb_image}, {_glow, &_glow_image}};
    for (auto &entry : images) {
        lv_image_cache_drop(&entry.image->dsc);
        lv_image_set_src(entry.obj, &entry.image->dsc);
        lv_obj_invalidate(entry.obj);
    }
    lv_obj_set_style_bg_color(_chip_dot, lv_color_hex(color), LV_PART_MAIN);
    if (_mode != Mode::Listening && _mode != Mode::Speaking) {
        _level_target = 0;
    }
    _face_valid = false;
    applyFace();
}

void XiaozhiUi::setLevel(float level)
{
    _level_target = clamp01(level);
}

void XiaozhiUi::setChatMessage(const char *role, const char *text)
{
    const bool user = role && strcmp(role, "user") == 0;
    const char *value = text ? text : "";
    if (user) {
        if (strcmp(_user_text, value) != 0) {
            copyText(_user_text, sizeof(_user_text), value);
            _reply_text[0] = '\0';      // A new question: the old answer goes
        }
    } else if (role && strcmp(role, "assistant") == 0) {
        copyText(_reply_text, sizeof(_reply_text), value);
    } else if (!value[0]) {
        _user_text[0] = '\0';
        _reply_text[0] = '\0';
    }
    updateSubtitle();
}

void XiaozhiUi::clearChatMessages()
{
    setChatMessage("system", "");
}

void XiaozhiUi::setHint(const char *hint)
{
    if (strcmp(_hint, hint ? hint : "") == 0) {
        return;
    }
    copyText(_hint, sizeof(_hint), hint);
    updateSubtitle();
}

void XiaozhiUi::updateSubtitle()
{
    if (!_reply_label) {
        return;
    }
    const char *reply = _reply_text;
    uint32_t color = 0xFFFFFF;
    if (_activation_visible && _activation_message[0]) {
        reply = _activation_message;
    } else if (!_reply_text[0] && !_user_text[0]) {
        reply = _hint;
        color = TEXT_DIM;
    }
    setLabelTextIfChanged(_user_label, _activation_visible ? "" : _user_text);
    setLabelTextIfChanged(_reply_label, reply);
    lv_obj_set_style_text_color(_reply_label, lv_color_hex(color), LV_PART_MAIN);
}

XiaozhiUi::Eyes XiaozhiUi::eyesForEmotion(const char *emotion)
{
    static constexpr struct {
        const char *name;
        Eyes eyes;
    } EMOTIONS[] = {
        {"happy", Eyes::Happy}, {"laughing", Eyes::Happy}, {"funny", Eyes::Happy}, {"loving", Eyes::Happy},
        {"kissy", Eyes::Happy}, {"delicious", Eyes::Happy}, {"relaxed", Eyes::Happy}, {"confident", Eyes::Happy},
        {"cool", Eyes::Happy}, {"silly", Eyes::Wink}, {"winking", Eyes::Wink}, {"surprised", Eyes::Round},
        {"shocked", Eyes::Round}, {"sad", Eyes::Sad}, {"crying", Eyes::Sad}, {"angry", Eyes::Squint},
        {"embarrassed", Eyes::Squint}, {"thinking", Eyes::Squint}, {"confused", Eyes::Squint},
        {"sleepy", Eyes::Closed},
    };
    for (const auto &entry : EMOTIONS) {
        if (emotion && strcmp(emotion, entry.name) == 0) {
            return entry.eyes;
        }
    }
    return Eyes::Open;
}

void XiaozhiUi::setEmotion(const char *emotion)
{
    const Eyes eyes = eyesForEmotion(emotion);
    if (_face_valid && eyes == _eyes) {
        return;
    }
    _eyes = eyes;
    _face_valid = false;
    applyFace();
}

void XiaozhiUi::applyFace()
{
    if (!_root || _face_valid) {
        return;
    }
    _face_valid = true;
    // Offline the face sleeps; while thinking it squints
    Eyes eyes = _eyes;
    if (_mode == Mode::Offline) {
        eyes = Eyes::Closed;
    } else if (_mode == Mode::Thinking && eyes == Eyes::Open) {
        eyes = Eyes::Squint;
    }
    const bool hide_face = _activation_visible;
    for (int i = 0; i < 2; i++) {
        bool pill = !hide_face;
        bool arc = false;
        int w = EYE_W;
        int h = EYE_H;
        int y = EYE_Y;
        switch (eyes) {
        case Eyes::Happy:
            pill = false;
            arc = !hide_face;
            lv_arc_set_angles(_eye_arcs[i], 200, 340);
            break;
        case Eyes::Round:
            w = h = 28;
            break;
        case Eyes::Sad:
            h = 24;
            y = EYE_Y + 6;
            break;
        case Eyes::Squint:
            h = i == 0 ? 12 : 28;
            break;
        case Eyes::Wink:
            if (i == 1) {
                pill = false;
                arc = !hide_face;
                lv_arc_set_angles(_eye_arcs[i], 200, 340);
            }
            break;
        case Eyes::Closed:
            pill = false;
            arc = !hide_face;
            lv_arc_set_angles(_eye_arcs[i], 30, 150);
            break;
        case Eyes::Open:
        default:
            break;
        }
        lv_obj_set_size(_eye_pills[i], w, h);
        lv_obj_align(_eye_pills[i], LV_ALIGN_CENTER, i == 0 ? -EYE_GAP : EYE_GAP, y);
        lv_obj_set_style_bg_opa(_eye_pills[i], eyes == Eyes::Sad ? LV_OPA_70 : LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_align(_eye_arcs[i], LV_ALIGN_CENTER, i == 0 ? -EYE_GAP : EYE_GAP,
                     eyes == Eyes::Closed ? EYE_Y - 8 : EYE_Y + 6);
        setHidden(_eye_pills[i], !pill);
        setHidden(_eye_arcs[i], !arc);
    }

    // The mouth: open with the voice while speaking, round while listening, a smile at rest
    Mouth mouth = Mouth::Smile;
    if (hide_face || _mode == Mode::Offline) {
        mouth = Mouth::None;
    } else if (_mode == Mode::Speaking) {
        mouth = Mouth::Open;
    } else if (_mode == Mode::Listening) {
        mouth = Mouth::Oval;
    } else if (_mode == Mode::Thinking || _mode == Mode::Busy) {
        mouth = Mouth::Flat;
    } else if (eyes == Eyes::Sad || _mode == Mode::Error) {
        mouth = Mouth::Frown;
    }
    setHidden(_mouth, !(mouth == Mouth::Open || mouth == Mouth::Oval || mouth == Mouth::Flat));
    setHidden(_mouth_arc, !(mouth == Mouth::Smile || mouth == Mouth::Frown));
    if (mouth == Mouth::Oval) {
        lv_obj_set_size(_mouth, 14, 14);
    } else if (mouth == Mouth::Flat) {
        lv_obj_set_size(_mouth, 18, 5);
    } else if (mouth == Mouth::Open) {
        lv_obj_set_size(_mouth, 24, 6);
    } else if (mouth == Mouth::Smile) {
        lv_arc_set_angles(_mouth_arc, 35, 145);
        lv_obj_align(_mouth_arc, LV_ALIGN_CENTER, 0, MOUTH_Y - 12);
    } else if (mouth == Mouth::Frown) {
        lv_arc_set_angles(_mouth_arc, 215, 325);
        lv_obj_align(_mouth_arc, LV_ALIGN_CENTER, 0, MOUTH_Y + 12);
    }
    setHidden(_code_label, !hide_face);
}

void XiaozhiUi::setActivation(const char *code, const char *message, bool visible)
{
    if (!_root) {
        return;
    }
    visible = visible && code && code[0];
    copyText(_activation_message, sizeof(_activation_message), visible ? message : "");
    setLabelTextIfChanged(_code_label, visible ? code : "");
    if (visible != _activation_visible) {
        _activation_visible = visible;
        _face_valid = false;
        applyFace();
    }
    updateSubtitle();
}

void XiaozhiUi::drawRing(lv_layer_t *layer)
{
    lv_area_t area;
    lv_obj_get_coords(_ring, &area);
    const float cx = (area.x1 + area.x2 + 1) / 2.0f;
    const float cy = (area.y1 + area.y2 + 1) / 2.0f;
    const lv_color_t color = lv_color_lighten(lv_color_hex(modeColor(_mode)), LV_OPA_20);

    if (_mode == Mode::Thinking || _mode == Mode::Busy) {
        // Three dots chasing each other around the orb
        lv_draw_rect_dsc_t dot;
        lv_draw_rect_dsc_init(&dot);
        dot.radius = LV_RADIUS_CIRCLE;
        dot.bg_color = color;
        for (int k = 0; k < 3; k++) {
            const float angle = _spin - k * 0.42f;
            const int size = 12 - k * 3;
            const float x = cx + cosf(angle) * (RING_INNER + 6);
            const float y = cy + sinf(angle) * (RING_INNER + 6);
            dot.bg_opa = static_cast<lv_opa_t>(255 - k * 70);
            const lv_area_t a = {static_cast<int32_t>(x - size / 2.0f), static_cast<int32_t>(y - size / 2.0f),
                                 static_cast<int32_t>(x + size / 2.0f), static_cast<int32_t>(y + size / 2.0f)};
            lv_draw_rect(layer, &dot, &a);
        }
        return;
    }
    if (_mode == Mode::Offline || _mode == Mode::Error || _mode == Mode::Activation || _activation_visible) {
        return;
    }

    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = color;
    line.width = 4;
    line.round_start = 1;
    line.round_end = 1;
    for (int i = 0; i < RING_BARS; i++) {
        const float angle = i * 2 * static_cast<float>(M_PI) / RING_BARS - static_cast<float>(M_PI) / 2;
        const float c = cosf(angle);
        const float s = sinf(angle);
        const float length = 3 + RING_REACH * _bars[i];
        line.opa = static_cast<lv_opa_t>(90 + 165 * clamp01(_bars[i] * 1.4f));
        line.p1.x = cx + c * RING_INNER;
        line.p1.y = cy + s * RING_INNER;
        line.p2.x = cx + c * (RING_INNER + length);
        line.p2.y = cy + s * (RING_INNER + length);
        lv_draw_line(layer, &line);
    }
}

void XiaozhiUi::animate()
{
    if (_toast_until && static_cast<int32_t>(lv_tick_get() - _toast_until) >= 0) {
        _toast_until = 0;
        lv_obj_set_style_text_color(_status_label, lv_color_hex(TEXT_CHIP), LV_PART_MAIN);
        setLabelTextIfChanged(_status_label, _status);
    }

    _level += (_level_target - _level) * (_level_target > _level ? 0.5f : 0.18f);
    _phase += TICK_MS / 1000.0f;
    _spin += TICK_MS / 1000.0f * 5.0f;
    const float breath = 0.5f - 0.5f * cosf(_phase * 2 * static_cast<float>(M_PI) / 3.2f);

    // The glow breathes at rest and brightens with the voice
    const bool alive = _mode != Mode::Offline && _mode != Mode::Error;
    const lv_opa_t glow = static_cast<lv_opa_t>(alive ? 150 + 60 * breath + 45 * _level : 110);
    if (lv_obj_get_style_image_opa(_glow, LV_PART_MAIN) != glow) {
        lv_obj_set_style_image_opa(_glow, glow, LV_PART_MAIN);
    }

    // Each bar follows the voice in its own rhythm; at rest they ripple faintly
    const bool voice = _mode == Mode::Listening || _mode == Mode::Speaking;
    for (int i = 0; i < RING_BARS; i++) {
        float target;
        if (voice) {
            const float wave = 0.5f + 0.5f * sinf(_phase * 7.3f + i * 0.85f) * sinf(_phase * 3.1f + i * 2.17f);
            target = _level * (0.35f + 0.65f * wave) + 0.04f;
        } else {
            target = 0.06f + 0.06f * breath * (0.5f + 0.5f * sinf(i * 0.52f + _phase * 1.6f));
        }
        _bars[i] += (target - _bars[i]) * 0.35f;
    }
    lv_obj_invalidate(_ring);

    // Speaking, the mouth opens with the voice
    if (_mode == Mode::Speaking && !_activation_visible) {
        const int h = 6 + static_cast<int>(16 * _level);
        if (lv_obj_get_height(_mouth) != h) {
            lv_obj_set_height(_mouth, h);
        }
    }

    // Blink now and then while the eyes are open pills
    const bool can_blink = !_activation_visible && _mode != Mode::Offline &&
                           (_eyes == Eyes::Open || _eyes == Eyes::Round || _eyes == Eyes::Sad);
    if (_blink_ms > TICK_MS) {
        _blink_ms -= TICK_MS;
    } else {
        // The blink is over: back to the open eyes
        _blink_ms = 2500 + esp_random() % 3500 + BLINK_MS;
        _face_valid = false;
        applyFace();
    }
    if (can_blink && _blink_ms <= BLINK_MS) {
        const float closed = blinkClosure(BLINK_MS - _blink_ms);
        for (auto *pill : _eye_pills) {
            const int open_h = _eyes == Eyes::Round ? 28 : (_eyes == Eyes::Sad ? 24 : EYE_H);
            lv_obj_set_height(pill, LV_MAX(4, static_cast<int>(open_h * (1 - closed))));
        }
    }
}

void XiaozhiUi::tickCallback(lv_timer_t *timer)
{
    auto *ui = static_cast<XiaozhiUi *>(lv_timer_get_user_data(timer));
    if (ui && ui->_root) {
        ui->animate();
    }
}

void XiaozhiUi::clickCallback(lv_event_t *event)
{
    auto *ui = static_cast<XiaozhiUi *>(lv_event_get_user_data(event));
    if (ui && ui->_action_callback) {
        ui->_action_callback(ui->_action_context);
    }
}

void XiaozhiUi::ringDrawCallback(lv_event_t *event)
{
    auto *ui = static_cast<XiaozhiUi *>(lv_event_get_user_data(event));
    if (ui && ui->_ring) {
        ui->drawRing(lv_event_get_layer(event));
    }
}

} // namespace esp_brookesia::apps
