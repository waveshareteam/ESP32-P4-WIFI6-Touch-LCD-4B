/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:App:Calculator"
#include "esp_lib_utils.h"
#include "esp_brookesia_app_calculator.hpp"

using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems;

LV_IMG_DECLARE(img_app_calculator);

namespace {

constexpr char APP_NAME[] = "Calculator";
constexpr int MAX_NUMBER_DIGITS = 15;
constexpr int MAX_FORMULA_CHARS = 64;

constexpr uint32_t COLOR_BACKGROUND = 0x0B0D12;
constexpr uint32_t COLOR_DISPLAY = 0x151923;
constexpr uint32_t COLOR_KEY = 0x242B38;
constexpr uint32_t COLOR_FUNCTION = 0x384152;
constexpr uint32_t COLOR_OPERATOR = 0xFF9F0A;
constexpr uint32_t COLOR_TEXT = 0xF7F8FA;
constexpr uint32_t COLOR_HINT = 0x9AA3B2;

const char *const keyboard_map[] = {
    "C", "/", "x", LV_SYMBOL_BACKSPACE, "\n",
    "7", "8", "9", "-", "\n",
    "4", "5", "6", "+", "\n",
    "1", "2", "3", "%", "\n",
    "0", ".", "=", ""
};

constexpr lv_style_selector_t ITEMS_DEFAULT =
    static_cast<lv_style_selector_t>(LV_PART_ITEMS) | static_cast<lv_style_selector_t>(LV_STATE_DEFAULT);

lv_color_t key_color(uint16_t id)
{
    if (id == 0 || id == 3) {
        return lv_color_hex(COLOR_FUNCTION);
    }
    if (id == 1 || id == 2 || id == 7 || id == 11 || id == 15 || id == 18) {
        return lv_color_hex(COLOR_OPERATOR);
    }
    return lv_color_hex(COLOR_KEY);
}

void format_result(double value, char *out, size_t size)
{
    if (!std::isfinite(value)) {
        snprintf(out, size, "Error");
        return;
    }
    if (value == 0) {
        value = 0; // Avoid displaying negative zero.
    }
    snprintf(out, size, "%.15g", value);
    if (strchr(out, 'e') != nullptr) {
        snprintf(out, size, "%.9g", value);
    }
}

void style_display_row(lv_obj_t *row)
{
    lv_obj_remove_style_all(row);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(row, 20, LV_PART_MAIN);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
}

} // namespace

namespace esp_brookesia::apps {

Calculator *Calculator::_instance = nullptr;

Calculator *Calculator::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (_instance == nullptr) {
        _instance = new Calculator(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

Calculator::Calculator(bool use_status_bar, bool use_navigation_bar) :
    App(APP_NAME, &img_app_calculator, true, use_status_bar, use_navigation_bar)
{
}

Calculator::~Calculator()
{
    ESP_UTILS_LOGD("Destroy(@0x%p)", this);
}

bool Calculator::init(void)
{
    return true;
}

bool Calculator::deinit(void)
{
    return true;
}

bool Calculator::run(void)
{
    _is_starting = true;

    const lv_area_t visual_area = getVisualArea();
    _width = lv_area_get_width(&visual_area);
    _height = lv_area_get_height(&visual_area);
    if (_width == 0 || _height == 0) {
        ESP_UTILS_LOGE("Invalid visual area");
        _is_starting = false;
        return false;
    }

    // The LCD-4B is square but not round. App's screen already matches the
    // visual area, so child coordinates deliberately start at (0, 0).
    const int margin = std::max(16, std::min<int>(_width / 24, 32));
    const int display_height = std::max<int>(_height * 30 / 100, 170);
    const int key_gap = std::max(8, std::min<int>(_width / 72, 14));
    const int keyboard_height = _height - display_height - margin * 2;
    const lv_font_t *formula_font = _width >= 600 ? &lv_font_montserrat_44 : &lv_font_montserrat_32;
    const lv_font_t *result_font = _width >= 600 ? &lv_font_montserrat_28 : &lv_font_montserrat_20;
    const lv_font_t *key_font = _width >= 600 ? &lv_font_montserrat_32 : &lv_font_montserrat_24;

    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *root = lv_obj_create(screen);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, _width, _height);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *display = lv_obj_create(root);
    lv_obj_remove_style_all(display);
    lv_obj_set_size(display, _width - margin * 2, display_height - margin / 2);
    lv_obj_set_pos(display, margin, margin / 2);
    lv_obj_set_style_bg_color(display, lv_color_hex(COLOR_DISPLAY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(display, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(display, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(display, margin / 2, LV_PART_MAIN);
    lv_obj_remove_flag(display, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *history_row = lv_obj_create(display);
    style_display_row(history_row);
    lv_obj_set_size(history_row, lv_pct(100), display_height * 22 / 100);
    lv_obj_align(history_row, LV_ALIGN_TOP_MID, 0, 0);
    history_label = lv_textarea_create(history_row);
    lv_obj_set_size(history_label, lv_pct(100), lv_pct(100));
    lv_obj_remove_style_all(history_label);
    lv_obj_set_style_text_font(history_label, result_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(history_label, lv_color_hex(COLOR_HINT), LV_PART_MAIN);
    lv_obj_set_style_text_align(history_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_opa(history_label, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_remove_flag(history_label, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_textarea_set_text(history_label, "");

    lv_obj_t *formula_row = lv_obj_create(display);
    style_display_row(formula_row);
    lv_obj_set_size(formula_row, lv_pct(100), display_height * 43 / 100);
    lv_obj_align(formula_row, LV_ALIGN_TOP_MID, 0, display_height * 20 / 100);
    formula_label = lv_label_create(formula_row);
    lv_obj_set_width(formula_label, lv_pct(100));
    lv_label_set_long_mode(formula_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_font(formula_label, formula_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(formula_label, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_align(formula_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(formula_label, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_label_set_text(formula_label, "0");

    lv_obj_t *result_row = lv_obj_create(display);
    style_display_row(result_row);
    lv_obj_set_size(result_row, lv_pct(100), display_height * 27 / 100);
    lv_obj_align(result_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    result_label = lv_label_create(result_row);
    lv_obj_set_width(result_label, lv_pct(100));
    lv_label_set_long_mode(result_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_font(result_label, result_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(result_label, lv_color_hex(COLOR_HINT), LV_PART_MAIN);
    lv_obj_set_style_text_align(result_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(result_label, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_label_set_text(result_label, "= 0");

    keyboard = lv_btnmatrix_create(root);
    lv_btnmatrix_set_map(keyboard, keyboard_map);
    lv_obj_set_size(keyboard, _width - margin * 2, keyboard_height);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, -margin);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(keyboard, key_gap, LV_PART_MAIN);
    lv_obj_set_style_pad_column(keyboard, key_gap, LV_PART_MAIN);
    lv_obj_set_style_radius(keyboard, 16, ITEMS_DEFAULT);
    lv_obj_set_style_border_width(keyboard, 0, ITEMS_DEFAULT);
    lv_obj_set_style_shadow_width(keyboard, 0, ITEMS_DEFAULT);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(COLOR_KEY), ITEMS_DEFAULT);
    lv_obj_set_style_text_color(keyboard, lv_color_hex(COLOR_TEXT), ITEMS_DEFAULT);
    lv_obj_set_style_text_font(keyboard, key_font, ITEMS_DEFAULT);

    // The button matrix has one shared item style. Its draw callback assigns
    // function and arithmetic colours by button index; zero spans two columns.
    lv_btnmatrix_set_btn_width(keyboard, 16, 2);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_ALL, this);

    formula_len = 1;
    _is_starting = false;
    return true;
}

bool Calculator::back(void)
{
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
    return true;
}

bool Calculator::close(void)
{
    _is_stopping = true;
    _is_stopping = false;
    return true;
}

bool Calculator::isStartZero(void)
{
    const char *text = lv_label_get_text(formula_label);
    const int text_len = strlen(text);
    return (text_len == 1 && text[0] == '0') ||
           (text_len >= 2 && text[text_len - 1] == '0' &&
            !std::isdigit(static_cast<unsigned char>(text[text_len - 2])) && text[text_len - 2] != '.');
}

bool Calculator::isStartNum(void)
{
    const char *text = lv_label_get_text(formula_label);
    const int text_len = strlen(text);
    return text_len > 0 && std::isdigit(static_cast<unsigned char>(text[text_len - 1]));
}

bool Calculator::isStartPercent(void)
{
    const char *text = lv_label_get_text(formula_label);
    const int text_len = strlen(text);
    return text_len > 0 && text[text_len - 1] == '%';
}

int Calculator::numberDigits(void)
{
    const char *text = lv_label_get_text(formula_label);
    int digits = 0;
    for (int i = strlen(text) - 1; i >= 0 &&
         (std::isdigit(static_cast<unsigned char>(text[i])) || text[i] == '.'); --i) {
        digits += text[i] != '.';
    }
    return digits;
}

bool Calculator::isLegalDot(void)
{
    const char *text = lv_label_get_text(formula_label);
    for (int index = strlen(text) - 1; index >= 0; --index) {
        if (text[index] == '.') {
            return false;
        }
        if (!std::isdigit(static_cast<unsigned char>(text[index]))) {
            return true;
        }
    }
    return true;
}

double Calculator::calculate(const char *input)
{
    std::vector<double> terms;
    double number = 0;
    bool after_dot = false;
    int decimal_places = 0;
    char pending_operator = '+';

    for (size_t i = 0, length = strlen(input); i < length; ++i) {
        const char token = input[i];
        if (token == '.') {
            after_dot = true;
        } else if (std::isdigit(static_cast<unsigned char>(token))) {
            if (after_dot) {
                number += (token - '0') / std::pow(10.0, ++decimal_places);
            } else {
                number = number * 10 + token - '0';
            }
        } else if (token == '%') {
            number /= 100.0;
        }

        if ((!std::isdigit(static_cast<unsigned char>(token)) && token != '.' && token != '%') || i + 1 == length) {
            switch (pending_operator) {
            case '+': terms.push_back(number); break;
            case '-': terms.push_back(-number); break;
            case 'x':
                if (terms.empty()) return NAN;
                terms.back() *= number;
                break;
            case '/':
                if (terms.empty() || number == 0) return NAN;
                terms.back() /= number;
                break;
            default: return NAN;
            }
            if (i + 1 < length) {
                pending_operator = token;
            }
            number = 0;
            after_dot = false;
            decimal_places = 0;
        }
    }

    double result = 0;
    for (double term : terms) {
        result += term;
    }
    return result;
}

void Calculator::keyboard_event_cb(lv_event_t *e)
{
    Calculator *app = static_cast<Calculator *>(lv_event_get_user_data(e));
    if (app == nullptr) {
        return;
    }

    if (lv_event_get_code(e) == LV_EVENT_DRAW_TASK_ADDED) {
        lv_draw_task_t *task = lv_event_get_draw_task(e);
        lv_draw_dsc_base_t *base = static_cast<lv_draw_dsc_base_t *>(lv_draw_task_get_draw_dsc(task));
        if (base == nullptr || base->part != LV_PART_ITEMS) {
            return;
        }
        lv_draw_fill_dsc_t *fill = lv_draw_task_get_fill_dsc(task);
        if (fill != nullptr) {
            const bool pressed = lv_btnmatrix_get_selected_btn(lv_event_get_target_obj(e)) == base->id1 &&
                                 lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_PRESSED);
            const lv_color_t color = key_color(base->id1);
            fill->color = pressed ? lv_color_lighten(color, LV_OPA_30) : color;
        }
        return;
    }

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    lv_obj_t *matrix = lv_event_get_target_obj(e);
    const uint16_t button = lv_btnmatrix_get_selected_btn(matrix);
    const char *button_text = lv_btnmatrix_get_btn_text(matrix, button);
    if (button == LV_BTNMATRIX_BTN_NONE || button_text == nullptr) {
        return;
    }

    bool update_result = false;
    bool equals = false;
    switch (button) {
    case 0: // C
        lv_label_set_text(app->formula_label, "0");
        app->formula_len = 1;
        update_result = true;
        break;
    case 3: // Backspace
        if (app->formula_len > 1) {
            lv_label_cut_text(app->formula_label, --app->formula_len, 1);
            update_result = true;
        } else if (!app->isStartZero()) {
            lv_label_set_text(app->formula_label, "0");
            app->formula_len = 1;
            update_result = true;
        }
        break;
    case 18: // =
        update_result = true;
        equals = true;
        break;
    case 1: case 2: case 7: case 11: case 15: // / x - + %
        if (app->formula_len < MAX_FORMULA_CHARS && (app->isStartNum() || app->isStartPercent())) {
            if ((button == 7 || button == 11) && app->isStartZero()) {
                lv_label_cut_text(app->formula_label, --app->formula_len, 1);
            }
            lv_label_ins_text(app->formula_label, app->formula_len++, button_text);
            update_result = button == 15;
        }
        break;
    case 4: case 5: case 6: case 8: case 9: case 10: case 12: case 13: case 14: case 16:
        if (app->formula_len < MAX_FORMULA_CHARS && app->numberDigits() < MAX_NUMBER_DIGITS && !app->isStartPercent()) {
            if (app->isStartZero()) {
                lv_label_cut_text(app->formula_label, --app->formula_len, 1);
            }
            lv_label_ins_text(app->formula_label, app->formula_len++, button_text);
            update_result = true;
        }
        break;
    case 17: // Decimal point
        if (app->formula_len < MAX_FORMULA_CHARS && app->isStartNum() && app->isLegalDot()) {
            lv_label_ins_text(app->formula_label, app->formula_len++, ".");
        }
        break;
    default:
        return;
    }

    if (!update_result) {
        return;
    }

    char result[32];
    format_result(app->calculate(lv_label_get_text(app->formula_label)), result, sizeof(result));
    lv_label_set_text_fmt(app->result_label, "= %s", result);

    if (equals) {
        char history[MAX_FORMULA_CHARS + sizeof(result) + 8];
        snprintf(history, sizeof(history), "%.64s = %.31s", lv_label_get_text(app->formula_label), result);
        lv_textarea_set_text(app->history_label, history);
        const bool can_continue = strchr(result, 'e') == nullptr && strcmp(result, "Error") != 0;
        lv_label_set_text(app->formula_label, can_continue ? result : "0");
        app->formula_len = strlen(lv_label_get_text(app->formula_label));
    }
}

} // namespace esp_brookesia::apps
