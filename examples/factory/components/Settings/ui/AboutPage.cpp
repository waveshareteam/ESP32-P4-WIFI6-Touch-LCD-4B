#include "AboutPage.hpp"
#include "SettingsUI.hpp"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "../Settings.hpp"

#include <stdio.h>

#define ESP_UTILS_LOG_TAG "BS:AboutPage"
#include "esp_lib_utils.h"

LV_IMG_DECLARE(logo);

namespace esp_brookesia::apps
{
    AboutPage *AboutPage::_instance = nullptr;

    AboutPage *AboutPage::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new AboutPage(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    AboutPage::AboutPage(bool use_status_bar, bool use_navigation_bar)
        : App("About", nullptr, true, use_status_bar, use_navigation_bar),
          page_root(nullptr), label(nullptr), list1(nullptr)
    {
    }

    AboutPage::~AboutPage()
    {
    }

    bool AboutPage::run()
    {
        ESP_UTILS_LOGD("AboutPage Run");
        lv_obj_clean(lv_scr_act());

        page_root = settings_ui::create_page(lv_scr_act());
        settings_ui::create_header(page_root, "About", [](lv_event_t *e) {
            (void)e;
            lv_async_call([](void *param) {
                (void)param;
                Settings::requestInstance()->showRootPage();
            }, nullptr);
        });

        settings_ui::init_list_styles(style_list, style_list_btn, style_list_text, style_list_btn_pressed);
        list1 = settings_ui::create_content_list(page_root);
        lv_obj_add_style(list1, &style_list, LV_PART_MAIN);

        settings_ui::add_section(list1, "Device", style_list_text);
        settings_ui::add_info_row(list1, &logo, "Manufacturer", "Waveshare", style_list_btn);
        settings_ui::add_info_row(
            list1, nullptr, "Product", "ESP32-P4-WIFI6-Touch-LCD-4B", style_list_btn);

        esp_chip_info_t chip = {};
        esp_chip_info(&chip);
        uint32_t flash_size = 0;
        (void)esp_flash_get_size(nullptr, &flash_size);

        char chip_name[32];
        snprintf(chip_name, sizeof(chip_name), "ESP32-P4 v%d.%d",
                 chip.revision / 100, chip.revision % 100);
        settings_ui::add_info_row(list1, nullptr, "Chip", chip_name, style_list_btn);

        char memory[64];
        snprintf(memory, sizeof(memory), "Flash %u MB / PSRAM %u MB",
                 static_cast<unsigned>(flash_size >> 20),
                 static_cast<unsigned>(esp_psram_get_size() >> 20));
        settings_ui::add_info_row(list1, nullptr, "Memory", memory, style_list_btn);

        settings_ui::add_section(list1, "Software", style_list_text);
        const esp_app_desc_t *app = esp_app_get_description();
        char firmware[64];
        snprintf(firmware, sizeof(firmware), "%s, %s", app->version, app->date);
        settings_ui::add_info_row(list1, nullptr, "Firmware", firmware, style_list_btn);
        char idf[48];
        snprintf(idf, sizeof(idf), "ESP-IDF %s", esp_get_idf_version());
        settings_ui::add_info_row(list1, nullptr, "Framework", idf, style_list_btn);
        settings_ui::add_info_row(list1, nullptr, "UI Framework", "ESP-Brookesia", style_list_btn);
        return true;
    }

    bool AboutPage::back()
    {
        ESP_UTILS_LOGD("AboutPage Back");
        Settings::requestInstance()->showRootPage();
        return true;
    }

    bool AboutPage::close()
    {
        ESP_UTILS_LOGD("AboutPage Close");
        if (page_root != nullptr)
        {
            lv_obj_del(page_root);
            page_root = nullptr;
            label = nullptr;
            list1 = nullptr;
            settings_ui::reset_list_styles(style_list, style_list_btn, style_list_text, style_list_btn_pressed);
        }
        return true;
    }

} // namespace esp_brookesia::apps
