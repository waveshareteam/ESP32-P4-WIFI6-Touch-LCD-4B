#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"
#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

class RelayControl: public systems::phone::App {
public:
    static RelayControl *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);
    ~RelayControl();

protected:
    RelayControl(bool use_status_bar, bool use_navigation_bar);

    bool run() override;
    bool back() override;
    bool close() override;
    bool init() override;
    bool deinit() override;
    bool pause() override;
    bool resume() override;

private:
    static RelayControl *_instance;

    lv_obj_t *_root = nullptr;
    lv_obj_t *_switches[2] = {};
    lv_obj_t *_state_labels[2] = {};
    lv_obj_t *_status_label = nullptr;
    bool _updating = false;
    esp_err_t _init_result = ESP_ERR_INVALID_STATE;
    esp_err_t _storage_result = ESP_ERR_INVALID_STATE;
    uint8_t _persisted_states = 0;
    bool _persisted_states_valid = false;

    void createUi();
    void destroyUi();
    void updateStatusUi();
    void syncRelayUi();
    void updateRelayUi(size_t index);
    void handleSwitch(lv_obj_t *target);
    uint8_t getRelayStates() const;
    esp_err_t loadPersistedStates(uint8_t &states, bool &needs_repair);
    esp_err_t persistRelayStates(uint8_t states);
    esp_err_t restoreRelayStates();

    static void switchEventCallback(lv_event_t *event);
};

} // namespace esp_brookesia::apps
