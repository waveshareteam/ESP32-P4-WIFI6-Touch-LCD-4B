/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "RelayControl.hpp"

#include "HardwareUI.hpp"
#include "nvs.h"
#include "bsp_board_peripherals.h"
#include "esp_brookesia.hpp"

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Relay"
#include "esp_lib_utils.h"

LV_IMG_DECLARE(img_app_relay_control);

namespace esp_brookesia::apps {
namespace {

constexpr size_t RELAY_COUNT = 2;
static_assert(RELAY_COUNT == BSP_EXTRA_RELAY_COUNT, "RelayControl and BSP relay counts must match");
constexpr uint8_t RELAY_STATE_MASK = (1U << RELAY_COUNT) - 1U;
constexpr char RELAY_NVS_NAMESPACE[] = "relay_control";
constexpr char RELAY_NVS_STATE_KEY[] = "output_state";

} // namespace

RelayControl *RelayControl::_instance = nullptr;

RelayControl *RelayControl::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (!_instance) {
        _instance = new RelayControl(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

RelayControl::RelayControl(bool use_status_bar, bool use_navigation_bar):
    App(
        "Relay Control",
        &img_app_relay_control,
        true,
        use_status_bar,
        use_navigation_bar
    )
{
}

RelayControl::~RelayControl()
{
    close();
}

bool RelayControl::run()
{
    _init_result = restoreRelayStates();
    createUi();
    return true;
}

bool RelayControl::back()
{
    return notifyCoreClosed();
}

bool RelayControl::close()
{
    destroyUi();
    return true;
}

bool RelayControl::init()
{
    _init_result = restoreRelayStates();
    return true;
}

uint8_t RelayControl::getRelayStates() const
{
    uint8_t states = 0;
    for (size_t i = 0; i < RELAY_COUNT; ++i) {
        if (bsp_extra_relay_get(i)) {
            states |= static_cast<uint8_t>(1U << i);
        }
    }
    return states;
}

esp_err_t RelayControl::loadPersistedStates(uint8_t &states, bool &needs_repair)
{
    states = 0;
    needs_repair = false;

    nvs_handle_t handle = 0;
    esp_err_t ret = nvs_open(RELAY_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_get_u8(handle, RELAY_NVS_STATE_KEY, &states);
    nvs_close(handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        states = 0;
        return ESP_OK;
    }
    if (ret == ESP_ERR_NVS_TYPE_MISMATCH) {
        states = 0;
        needs_repair = true;
        return ret;
    }
    if (ret != ESP_OK) {
        states = 0;
        return ret;
    }
    if ((states & static_cast<uint8_t>(~RELAY_STATE_MASK)) != 0) {
        needs_repair = true;
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

esp_err_t RelayControl::persistRelayStates(uint8_t states)
{
    if ((states & static_cast<uint8_t>(~RELAY_STATE_MASK)) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (_persisted_states_valid && _persisted_states == states) {
        return ESP_OK;
    }

    nvs_handle_t handle = 0;
    esp_err_t ret = nvs_open(RELAY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    if (!_persisted_states_valid) {
        ret = nvs_erase_key(handle, RELAY_NVS_STATE_KEY);
        if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
            nvs_close(handle);
            return ret;
        }
    }

    ret = nvs_set_u8(handle, RELAY_NVS_STATE_KEY, states);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);

    if (ret == ESP_OK) {
        _persisted_states = states;
        _persisted_states_valid = true;
    }
    return ret;
}

esp_err_t RelayControl::restoreRelayStates()
{
    uint8_t states = 0;
    bool needs_repair = false;
    esp_err_t load_result = loadPersistedStates(states, needs_repair);
    if (load_result == ESP_OK) {
        _persisted_states = states;
        _persisted_states_valid = true;
        _storage_result = ESP_OK;
    } else {
        _persisted_states = 0;
        _persisted_states_valid = false;
        _storage_result = load_result;
        states = 0;
        ESP_UTILS_LOGW(
            "Relay state is unavailable or invalid (%s); using safe OFF defaults",
            esp_err_to_name(load_result)
        );
    }

    esp_err_t gpio_result = bsp_extra_relay_init();
    if (gpio_result == ESP_OK) {
        for (size_t i = 0; i < RELAY_COUNT; ++i) {
            esp_err_t ret = bsp_extra_relay_set(i, (states & (1U << i)) != 0);
            if (ret != ESP_OK && gpio_result == ESP_OK) {
                gpio_result = ret;
            }
        }
    }

    if (needs_repair) {
        esp_err_t repair_result = persistRelayStates(0);
        _storage_result = repair_result;
        if (repair_result != ESP_OK) {
            ESP_UTILS_LOGW("Repair relay state failed: %s", esp_err_to_name(repair_result));
        }
    }

    if (gpio_result == ESP_OK) {
        ESP_UTILS_LOGI("Restored relay state mask: 0x%02x", static_cast<unsigned>(states));
    }
    return gpio_result;
}

bool RelayControl::deinit()
{
    destroyUi();
    return true;
}

bool RelayControl::pause()
{
    return true;
}

bool RelayControl::resume()
{
    _init_result = restoreRelayStates();
    if (!_root) {
        createUi();
    } else {
        syncRelayUi();
    }
    return true;
}

void RelayControl::createUi()
{
    destroyUi();

    lv_obj_t *content = nullptr;
    _root = hardware_ui::create_page("Relay Control", &content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 16, LV_PART_MAIN);

    _status_label = lv_label_create(content);
    lv_obj_set_width(_status_label, lv_pct(100));
    lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_18, LV_PART_MAIN);
    updateStatusUi();

    static const char *names[RELAY_COUNT] = {"Relay 1", "Relay 2"};
    static const char *pins[RELAY_COUNT] = {"GPIO32", "GPIO46"};

    for (size_t i = 0; i < RELAY_COUNT; ++i) {
        lv_obj_t *panel = hardware_ui::create_panel(content);
        lv_obj_set_size(panel, lv_pct(100), 188);
        lv_obj_set_style_pad_all(panel, 24, LV_PART_MAIN);
        lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(panel, 22, LV_PART_MAIN);

        lv_obj_t *power_icon = lv_obj_create(panel);
        lv_obj_set_size(power_icon, 72, 72);
        lv_obj_set_style_radius(power_icon, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(power_icon, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(power_icon, lv_color_hex(hardware_ui::COLOR_SURFACE_ALT), LV_PART_MAIN);
        lv_obj_clear_flag(power_icon, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *icon_label = lv_label_create(power_icon);
        lv_label_set_text(icon_label, LV_SYMBOL_POWER);
        lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_30, LV_PART_MAIN);
        lv_obj_set_style_text_color(icon_label, lv_color_hex(hardware_ui::COLOR_PRIMARY_TEXT), LV_PART_MAIN);
        lv_obj_center(icon_label);

        lv_obj_t *text_column = lv_obj_create(panel);
        lv_obj_remove_style_all(text_column);
        lv_obj_set_height(text_column, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(text_column, 1);
        lv_obj_set_flex_flow(text_column, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(text_column, 8, LV_PART_MAIN);
        lv_obj_clear_flag(text_column, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name_label = lv_label_create(text_column);
        lv_label_set_text(name_label, names[i]);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_28, LV_PART_MAIN);
        lv_obj_set_style_text_color(name_label, lv_color_hex(hardware_ui::COLOR_PRIMARY_TEXT), LV_PART_MAIN);

        lv_obj_t *detail_label = lv_label_create(text_column);
        lv_label_set_text(detail_label, pins[i]);
        hardware_ui::style_secondary_label(detail_label);

        _state_labels[i] = lv_label_create(text_column);
        lv_obj_set_style_text_font(_state_labels[i], &lv_font_montserrat_20, LV_PART_MAIN);

        _switches[i] = lv_switch_create(panel);
        lv_obj_set_size(_switches[i], 92, 48);
        if (bsp_extra_relay_get(i)) {
            lv_obj_add_state(_switches[i], LV_STATE_CHECKED);
        }
        lv_obj_set_style_bg_color(
            _switches[i],
            lv_color_hex(hardware_ui::COLOR_SUCCESS),
            LV_PART_INDICATOR | LV_STATE_CHECKED
        );
        lv_obj_add_event_cb(_switches[i], switchEventCallback, LV_EVENT_VALUE_CHANGED, this);
        updateRelayUi(i);
    }
}

void RelayControl::updateStatusUi()
{
    if (!_status_label) {
        return;
    }

    const bool healthy = _init_result == ESP_OK && _storage_result == ESP_OK;
    lv_obj_set_style_text_color(
        _status_label,
        lv_color_hex(healthy ? hardware_ui::COLOR_SECONDARY_TEXT : hardware_ui::COLOR_ERROR),
        LV_PART_MAIN
    );
    if (_init_result != ESP_OK) {
        lv_label_set_text_fmt(
            _status_label, "Relay GPIO initialization failed: %s", esp_err_to_name(_init_result)
        );
    } else if (_storage_result != ESP_OK) {
        lv_label_set_text_fmt(
            _status_label, "Relay state restore failed: %s", esp_err_to_name(_storage_result)
        );
    } else {
        lv_label_set_text(_status_label, "GPIO32 / GPIO46");
    }
}

void RelayControl::syncRelayUi()
{
    _updating = true;
    for (size_t i = 0; i < RELAY_COUNT; ++i) {
        if (!_switches[i]) {
            continue;
        }
        if (bsp_extra_relay_get(i)) {
            lv_obj_add_state(_switches[i], LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(_switches[i], LV_STATE_CHECKED);
        }
    }
    _updating = false;

    updateStatusUi();
    for (size_t i = 0; i < RELAY_COUNT; ++i) {
        updateRelayUi(i);
    }
}

void RelayControl::destroyUi()
{
    if (_root) {
        lv_obj_delete(_root);
        _root = nullptr;
    }
    _status_label = nullptr;
    for (size_t i = 0; i < RELAY_COUNT; ++i) {
        _switches[i] = nullptr;
        _state_labels[i] = nullptr;
    }
}

void RelayControl::updateRelayUi(size_t index)
{
    if (index >= RELAY_COUNT || !_state_labels[index]) {
        return;
    }

    bool enabled = bsp_extra_relay_get(index);
    lv_label_set_text(_state_labels[index], enabled ? "ON" : "OFF");
    lv_obj_set_style_text_color(
        _state_labels[index],
        lv_color_hex(enabled ? hardware_ui::COLOR_SUCCESS : hardware_ui::COLOR_SECONDARY_TEXT),
        LV_PART_MAIN
    );
}

void RelayControl::handleSwitch(lv_obj_t *target)
{
    if (_updating) {
        return;
    }

    for (size_t i = 0; i < RELAY_COUNT; ++i) {
        if (_switches[i] != target) {
            continue;
        }

        bool enabled = lv_obj_has_state(target, LV_STATE_CHECKED);
        esp_err_t ret = bsp_extra_relay_set(i, enabled);
        if (ret != ESP_OK) {
            _updating = true;
            if (enabled) {
                lv_obj_remove_state(target, LV_STATE_CHECKED);
            } else {
                lv_obj_add_state(target, LV_STATE_CHECKED);
            }
            _updating = false;
            if (_status_label) {
                lv_label_set_text_fmt(_status_label, "Relay %u error: %s", (unsigned)(i + 1), esp_err_to_name(ret));
                lv_obj_set_style_text_color(
                    _status_label,
                    lv_color_hex(hardware_ui::COLOR_ERROR),
                    LV_PART_MAIN
                );
            }
        } else {
            esp_err_t storage_ret = persistRelayStates(getRelayStates());
            _storage_result = storage_ret;
            if (_status_label) {
                if (storage_ret == ESP_OK) {
                    lv_label_set_text(_status_label, "GPIO32 / GPIO46");
                    lv_obj_set_style_text_color(
                        _status_label,
                        lv_color_hex(hardware_ui::COLOR_SECONDARY_TEXT),
                        LV_PART_MAIN
                    );
                } else {
                    lv_label_set_text_fmt(
                        _status_label, "Relay state save failed: %s", esp_err_to_name(storage_ret)
                    );
                    lv_obj_set_style_text_color(
                        _status_label,
                        lv_color_hex(hardware_ui::COLOR_ERROR),
                        LV_PART_MAIN
                    );
                }
            }
        }
        updateRelayUi(i);
        return;
    }
}

void RelayControl::switchEventCallback(lv_event_t *event)
{
    auto *instance = static_cast<RelayControl *>(lv_event_get_user_data(event));
    if (instance) {
        instance->handleSwitch(static_cast<lv_obj_t *>(lv_event_get_target(event)));
    }
}

} // namespace esp_brookesia::apps
