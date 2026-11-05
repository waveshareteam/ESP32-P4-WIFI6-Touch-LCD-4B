/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "EthernetInfo.hpp"

#include <stdio.h>
#include <string.h>

#include "HardwareUI.hpp"
#include "bsp_board_peripherals.h"
#include "esp_brookesia.hpp"
#include "esp_netif_ip_addr.h"

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Ethernet"
#include "esp_lib_utils.h"

LV_IMG_DECLARE(img_app_ethernet);

namespace esp_brookesia::apps {

namespace {

lv_obj_t *addInfoRow(lv_obj_t *parent, const char *name, lv_obj_t **value_out)
{
    lv_obj_t *row = hardware_ui::create_panel(parent);
    lv_obj_set_size(row, lv_pct(100), 72);
    lv_obj_set_style_pad_hor(row, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(row, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *name_label = lv_label_create(row);
    lv_label_set_text(name_label, name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(name_label, lv_color_hex(hardware_ui::COLOR_PRIMARY_TEXT), LV_PART_MAIN);

    lv_obj_t *value_label = lv_label_create(row);
    lv_obj_set_width(value_label, lv_pct(64));
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    hardware_ui::style_secondary_label(value_label, &lv_font_montserrat_20);
    lv_label_set_text(value_label, "--");
    *value_out = value_label;
    return row;
}

void setLabelIfChanged(lv_obj_t *label, const char *text)
{
    if (label && strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

} // namespace

EthernetInfo *EthernetInfo::_instance = nullptr;

EthernetInfo *EthernetInfo::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (!_instance) {
        _instance = new EthernetInfo(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

EthernetInfo::EthernetInfo(bool use_status_bar, bool use_navigation_bar):
    App(
        "Ethernet",
        &img_app_ethernet,
        true,
        use_status_bar,
        use_navigation_bar
    )
{
}

EthernetInfo::~EthernetInfo()
{
    close();
}

bool EthernetInfo::run()
{
    if (_init_result != ESP_OK) {
        _init_result = bsp_extra_ethernet_init();
    }
    createUi();
    refresh();
    return true;
}

bool EthernetInfo::back()
{
    return notifyCoreClosed();
}

bool EthernetInfo::close()
{
    destroyUi();
    return true;
}

bool EthernetInfo::init()
{
    _init_result = bsp_extra_ethernet_init();
    return true;
}

bool EthernetInfo::deinit()
{
    destroyUi();
    return true;
}

bool EthernetInfo::pause()
{
    if (_refresh_timer) {
        lv_timer_pause(_refresh_timer);
    }
    return true;
}

bool EthernetInfo::resume()
{
    if (_refresh_timer) {
        lv_timer_resume(_refresh_timer);
    }
    return true;
}

void EthernetInfo::createUi()
{
    destroyUi();

    lv_obj_t *content = nullptr;
    _root = hardware_ui::create_page("Ethernet", &content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 10, LV_PART_MAIN);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *status_panel = hardware_ui::create_panel(content);
    lv_obj_set_size(status_panel, lv_pct(100), 92);
    lv_obj_set_style_pad_hor(status_panel, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(status_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        status_panel,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );
    lv_obj_set_style_pad_column(status_panel, 16, LV_PART_MAIN);

    _status_dot = lv_obj_create(status_panel);
    lv_obj_set_size(_status_dot, 18, 18);
    lv_obj_set_style_radius(_status_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(_status_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_status_dot, lv_color_hex(hardware_ui::COLOR_WARNING), LV_PART_MAIN);
    lv_obj_clear_flag(_status_dot, LV_OBJ_FLAG_SCROLLABLE);

    _status_label = lv_label_create(status_panel);
    lv_obj_set_flex_grow(_status_label, 1);
    lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_status_label, lv_color_hex(hardware_ui::COLOR_PRIMARY_TEXT), LV_PART_MAIN);
    lv_label_set_text(_status_label, "Starting");

    lv_obj_t *refresh_button = hardware_ui::create_icon_button(status_panel, LV_SYMBOL_REFRESH);
    lv_obj_add_event_cb(refresh_button, refreshButtonCallback, LV_EVENT_CLICKED, this);

    static const char *row_names[6] = {
        "IPv4 address",
        "Netmask",
        "Gateway",
        "MAC address",
        "Link",
        "PHY",
    };
    for (size_t i = 0; i < 6; ++i) {
        addInfoRow(content, row_names[i], &_value_labels[i]);
    }

    _refresh_timer = lv_timer_create(refreshTimerCallback, 500, this);
}

void EthernetInfo::destroyUi()
{
    if (_refresh_timer) {
        lv_timer_delete(_refresh_timer);
        _refresh_timer = nullptr;
    }
    if (_root) {
        lv_obj_delete(_root);
        _root = nullptr;
    }
    _status_dot = nullptr;
    _status_label = nullptr;
    for (auto &label : _value_labels) {
        label = nullptr;
    }
}

void EthernetInfo::refresh()
{
    if (!_root) {
        return;
    }

    bsp_extra_ethernet_info_t info = {};
    bsp_extra_ethernet_get_info(&info);

    const char *status = "Starting";
    uint32_t status_color = hardware_ui::COLOR_WARNING;
    if (_init_result != ESP_OK || info.last_error != ESP_OK) {
        status = "Initialization error";
        status_color = hardware_ui::COLOR_ERROR;
    } else if (!info.link_up) {
        status = "Cable disconnected";
        status_color = hardware_ui::COLOR_SECONDARY_TEXT;
    } else if (!info.ip_acquired) {
        status = "Link up / DHCP";
        status_color = hardware_ui::COLOR_WARNING;
    } else {
        status = "Connected";
        status_color = hardware_ui::COLOR_SUCCESS;
    }
    setLabelIfChanged(_status_label, status);
    lv_obj_set_style_bg_color(_status_dot, lv_color_hex(status_color), LV_PART_MAIN);

    char buffer[64];
    if (info.ip_acquired) {
        snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&info.ip_info.ip));
        setLabelIfChanged(_value_labels[0], buffer);
        snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&info.ip_info.netmask));
        setLabelIfChanged(_value_labels[1], buffer);
        snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&info.ip_info.gw));
        setLabelIfChanged(_value_labels[2], buffer);
    } else {
        setLabelIfChanged(_value_labels[0], "--");
        setLabelIfChanged(_value_labels[1], "--");
        setLabelIfChanged(_value_labels[2], "--");
    }

    snprintf(
        buffer,
        sizeof(buffer),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        info.mac[0],
        info.mac[1],
        info.mac[2],
        info.mac[3],
        info.mac[4],
        info.mac[5]
    );
    setLabelIfChanged(_value_labels[3], buffer);

    if (info.link_up) {
        snprintf(
            buffer,
            sizeof(buffer),
            "%u Mbps / %s",
            (unsigned)info.speed_mbps,
            info.full_duplex ? "full duplex" : "half duplex"
        );
        setLabelIfChanged(_value_labels[4], buffer);
    } else {
        setLabelIfChanged(_value_labels[4], "--");
    }
    setLabelIfChanged(_value_labels[5], "IP101 / address 1");
}

void EthernetInfo::refreshTimerCallback(lv_timer_t *timer)
{
    auto *instance = static_cast<EthernetInfo *>(lv_timer_get_user_data(timer));
    if (instance) {
        instance->refresh();
    }
}

void EthernetInfo::refreshButtonCallback(lv_event_t *event)
{
    auto *instance = static_cast<EthernetInfo *>(lv_event_get_user_data(event));
    if (instance) {
        instance->_init_result = bsp_extra_ethernet_init();
        instance->refresh();
    }
}

} // namespace esp_brookesia::apps
