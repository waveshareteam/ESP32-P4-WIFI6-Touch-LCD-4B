#pragma once

#include "lvgl.h"
#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

class EthernetInfo: public systems::phone::App {
public:
    static EthernetInfo *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);
    ~EthernetInfo();

protected:
    EthernetInfo(bool use_status_bar, bool use_navigation_bar);

    bool run() override;
    bool back() override;
    bool close() override;
    bool init() override;
    bool deinit() override;
    bool pause() override;
    bool resume() override;

private:
    static EthernetInfo *_instance;

    lv_obj_t *_root = nullptr;
    lv_obj_t *_status_dot = nullptr;
    lv_obj_t *_status_label = nullptr;
    lv_obj_t *_value_labels[6] = {};
    lv_timer_t *_refresh_timer = nullptr;
    esp_err_t _init_result = ESP_ERR_INVALID_STATE;

    void createUi();
    void destroyUi();
    void refresh();

    static void refreshTimerCallback(lv_timer_t *timer);
    static void refreshButtonCallback(lv_event_t *event);
};

} // namespace esp_brookesia::apps
