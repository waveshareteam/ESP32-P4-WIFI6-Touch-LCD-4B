/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "RS485Terminal.hpp"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "HardwareUI.hpp"
#include "bsp_board_peripherals.h"
#include "esp_brookesia.hpp"

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:RS485"
#include "esp_lib_utils.h"

LV_IMG_DECLARE(img_app_rs485);

namespace esp_brookesia::apps {

namespace {

static constexpr char AUTO_MESSAGE[] =
    "Hi,this is Waveshare ESP32-P4-WIFI6-Touch-LCD-4B!\r\n";

void setLabelIfChanged(lv_obj_t *label, const char *text)
{
    if (label && strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

} // namespace

RS485Terminal *RS485Terminal::_instance = nullptr;

RS485Terminal *RS485Terminal::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (!_instance) {
        _instance = new RS485Terminal(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

RS485Terminal::RS485Terminal(bool use_status_bar, bool use_navigation_bar):
    App(
        "RS485",
        &img_app_rs485,
        true,
        use_status_bar,
        use_navigation_bar
    )
{
}

RS485Terminal::~RS485Terminal()
{
    deinit();
}

bool RS485Terminal::run()
{
    if (_init_result != ESP_OK) {
        _init_result = bsp_extra_rs485_init();
    }
    createUi();
    startWorker();
    return true;
}

bool RS485Terminal::back()
{
    stopWorker();
    return notifyCoreClosed();
}

bool RS485Terminal::close()
{
    stopWorker();
    destroyUi();
    return true;
}

bool RS485Terminal::init()
{
    if (!_rx_mutex) {
        _rx_mutex = xSemaphoreCreateMutex();
    }
    if (!_rx_mutex) {
        _init_result = ESP_ERR_NO_MEM;
        return false;
    }
    _init_result = bsp_extra_rs485_init();
    return true;
}

bool RS485Terminal::deinit()
{
    stopWorker();
    destroyUi();
    if (_worker_task) {
        ESP_UTILS_LOGE("RS485 worker did not stop before deinit");
        return false;
    }
    if (_rx_mutex) {
        vSemaphoreDelete(_rx_mutex);
        _rx_mutex = nullptr;
    }
    return true;
}

bool RS485Terminal::pause()
{
    stopWorker();
    return true;
}

bool RS485Terminal::resume()
{
    startWorker();
    return true;
}

void RS485Terminal::createUi()
{
    destroyUi();

    lv_obj_t *content = nullptr;
    _root = hardware_ui::create_page("RS485 Terminal", &content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 14, LV_PART_MAIN);

    lv_obj_t *tx_panel = hardware_ui::create_panel(content);
    lv_obj_set_size(tx_panel, lv_pct(100), 198);
    lv_obj_set_style_pad_all(tx_panel, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(tx_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tx_panel, 12, LV_PART_MAIN);

    lv_obj_t *tx_header = lv_obj_create(tx_panel);
    lv_obj_remove_style_all(tx_header);
    lv_obj_set_size(tx_header, lv_pct(100), 42);
    lv_obj_set_flex_flow(tx_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tx_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(tx_header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *tx_title = lv_label_create(tx_header);
    lv_label_set_text(tx_title, "Transmit");
    lv_obj_set_style_text_font(tx_title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(tx_title, lv_color_hex(hardware_ui::COLOR_PRIMARY_TEXT), LV_PART_MAIN);

    _auto_switch = lv_switch_create(tx_header);
    lv_obj_set_size(_auto_switch, 82, 42);
    if (_auto_send.load()) {
        lv_obj_add_state(_auto_switch, LV_STATE_CHECKED);
    }
    lv_obj_set_style_bg_color(
        _auto_switch,
        lv_color_hex(hardware_ui::COLOR_SUCCESS),
        LV_PART_INDICATOR | LV_STATE_CHECKED
    );
    lv_obj_add_event_cb(_auto_switch, autoSwitchCallback, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t *message_label = lv_label_create(tx_panel);
    lv_obj_set_width(message_label, lv_pct(100));
    lv_label_set_long_mode(message_label, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(message_label, "Hi,this is Waveshare ESP32-P4-WIFI6-Touch-LCD-4B!");
    lv_obj_set_style_text_font(message_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(message_label, lv_color_hex(hardware_ui::COLOR_PRIMARY_TEXT), LV_PART_MAIN);

    _tx_count_label = lv_label_create(tx_panel);
    hardware_ui::style_secondary_label(_tx_count_label);
    lv_label_set_text(_tx_count_label, "TX frames: 0");

    lv_obj_t *rx_panel = hardware_ui::create_panel(content);
    lv_obj_set_width(rx_panel, lv_pct(100));
    lv_obj_set_flex_grow(rx_panel, 1);
    lv_obj_set_style_pad_all(rx_panel, 18, LV_PART_MAIN);
    lv_obj_set_flex_flow(rx_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(rx_panel, 10, LV_PART_MAIN);

    lv_obj_t *rx_header = lv_obj_create(rx_panel);
    lv_obj_remove_style_all(rx_header);
    lv_obj_set_size(rx_header, lv_pct(100), 48);
    lv_obj_set_flex_flow(rx_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rx_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(rx_header, LV_OBJ_FLAG_SCROLLABLE);

    _rx_count_label = lv_label_create(rx_header);
    lv_obj_set_style_text_font(_rx_count_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_rx_count_label, lv_color_hex(hardware_ui::COLOR_PRIMARY_TEXT), LV_PART_MAIN);
    lv_label_set_text(_rx_count_label, "Receive / 0 bytes");

    lv_obj_t *clear_button = hardware_ui::create_icon_button(rx_header, LV_SYMBOL_TRASH);
    lv_obj_add_event_cb(clear_button, clearButtonCallback, LV_EVENT_CLICKED, this);

    lv_obj_t *rx_scroll = lv_obj_create(rx_panel);
    lv_obj_remove_style_all(rx_scroll);
    lv_obj_set_width(rx_scroll, lv_pct(100));
    lv_obj_set_flex_grow(rx_scroll, 1);
    lv_obj_set_scroll_dir(rx_scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(rx_scroll, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(rx_scroll, lv_color_hex(0x101012), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rx_scroll, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(rx_scroll, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rx_scroll, 14, LV_PART_MAIN);

    _rx_text_label = lv_label_create(rx_scroll);
    lv_obj_set_width(_rx_text_label, lv_pct(100));
    lv_label_set_long_mode(_rx_text_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_font(_rx_text_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(_rx_text_label, lv_color_hex(hardware_ui::COLOR_PRIMARY_TEXT), LV_PART_MAIN);
    lv_label_set_text(_rx_text_label, _init_result == ESP_OK ? "Waiting for data..." : "UART initialization failed");

    _ui_timer = lv_timer_create(uiTimerCallback, 200, this);
    refreshUi();
}

void RS485Terminal::destroyUi()
{
    if (_ui_timer) {
        lv_timer_delete(_ui_timer);
        _ui_timer = nullptr;
    }
    if (_root) {
        lv_obj_delete(_root);
        _root = nullptr;
    }
    _auto_switch = nullptr;
    _tx_count_label = nullptr;
    _rx_count_label = nullptr;
    _rx_text_label = nullptr;
}

bool RS485Terminal::startWorker()
{
    if (_worker_task) {
        return true;
    }
    if (_init_result != ESP_OK) {
        return false;
    }

    _worker_running.store(true);
    BaseType_t result = xTaskCreatePinnedToCore(
        workerEntry,
        "rs485_terminal",
        4096,
        this,
        5,
        &_worker_task,
        0
    );
    if (result != pdPASS) {
        _worker_running.store(false);
        _worker_task = nullptr;
        return false;
    }
    return true;
}

void RS485Terminal::stopWorker()
{
    if (!_worker_task) {
        return;
    }

    _worker_running.store(false);
    TickType_t start = xTaskGetTickCount();
    while (_worker_task && xTaskGetCurrentTaskHandle() != _worker_task &&
           (xTaskGetTickCount() - start) < pdMS_TO_TICKS(1000)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void RS485Terminal::worker()
{
    TickType_t next_send = xTaskGetTickCount();
    uint8_t receive_buffer[128];

    while (_worker_running.load()) {
        TickType_t now = xTaskGetTickCount();
        if (_auto_send.load() && (int32_t)(now - next_send) >= 0) {
            size_t bytes_written = 0;
            if (bsp_extra_rs485_write(
                    AUTO_MESSAGE,
                    sizeof(AUTO_MESSAGE) - 1,
                    &bytes_written
                ) == ESP_OK && bytes_written == sizeof(AUTO_MESSAGE) - 1) {
                _tx_count.fetch_add(1);
            }
            next_send = now + pdMS_TO_TICKS(AUTO_SEND_PERIOD_MS);
        }

        size_t bytes_read = 0;
        if (bsp_extra_rs485_read(
                receive_buffer,
                sizeof(receive_buffer),
                &bytes_read,
                20
            ) == ESP_OK && bytes_read > 0) {
            appendReceived(receive_buffer, bytes_read);
            _rx_count.fetch_add(bytes_read);
        }
    }

    _worker_task = nullptr;
    vTaskDelete(nullptr);
}

void RS485Terminal::appendReceived(const uint8_t *data, size_t size)
{
    if (!_rx_mutex || xSemaphoreTake(_rx_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    for (size_t i = 0; i < size; ++i) {
        char encoded[5] = {};
        size_t encoded_length = 0;
        if (data[i] == '\r' || data[i] == '\n' || data[i] == '\t' || isprint(data[i])) {
            encoded[0] = static_cast<char>(data[i]);
            encoded_length = 1;
        } else {
            snprintf(encoded, sizeof(encoded), "\\x%02X", data[i]);
            encoded_length = 4;
        }

        if (_rx_text_length + encoded_length + 1 >= RX_TEXT_CAPACITY) {
            size_t keep = RX_TEXT_CAPACITY / 2;
            memmove(_rx_text, _rx_text + _rx_text_length - keep, keep);
            _rx_text_length = keep;
            _rx_text[_rx_text_length] = '\0';
        }

        memcpy(_rx_text + _rx_text_length, encoded, encoded_length);
        _rx_text_length += encoded_length;
        _rx_text[_rx_text_length] = '\0';
    }

    xSemaphoreGive(_rx_mutex);
}

void RS485Terminal::refreshUi()
{
    char counter[48];
    snprintf(counter, sizeof(counter), "TX frames: %lu", (unsigned long)_tx_count.load());
    setLabelIfChanged(_tx_count_label, counter);
    snprintf(counter, sizeof(counter), "Receive / %lu bytes", (unsigned long)_rx_count.load());
    setLabelIfChanged(_rx_count_label, counter);

    if (!_rx_text_label || !_rx_mutex || xSemaphoreTake(_rx_mutex, 0) != pdTRUE) {
        return;
    }
    if (_rx_text_length > 0) {
        setLabelIfChanged(_rx_text_label, _rx_text);
    }
    xSemaphoreGive(_rx_mutex);
}

void RS485Terminal::clearReceived()
{
    if (!_rx_mutex || xSemaphoreTake(_rx_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    _rx_text[0] = '\0';
    _rx_text_length = 0;
    _rx_count.store(0);
    xSemaphoreGive(_rx_mutex);

    if (_rx_text_label) {
        lv_label_set_text(_rx_text_label, "Waiting for data...");
    }
}

void RS485Terminal::workerEntry(void *arg)
{
    static_cast<RS485Terminal *>(arg)->worker();
}

void RS485Terminal::uiTimerCallback(lv_timer_t *timer)
{
    auto *instance = static_cast<RS485Terminal *>(lv_timer_get_user_data(timer));
    if (instance) {
        instance->refreshUi();
    }
}

void RS485Terminal::autoSwitchCallback(lv_event_t *event)
{
    auto *instance = static_cast<RS485Terminal *>(lv_event_get_user_data(event));
    if (instance) {
        auto *target = static_cast<lv_obj_t *>(lv_event_get_target(event));
        instance->_auto_send.store(lv_obj_has_state(target, LV_STATE_CHECKED));
    }
}

void RS485Terminal::clearButtonCallback(lv_event_t *event)
{
    auto *instance = static_cast<RS485Terminal *>(lv_event_get_user_data(event));
    if (instance) {
        instance->clearReceived();
    }
}

} // namespace esp_brookesia::apps
