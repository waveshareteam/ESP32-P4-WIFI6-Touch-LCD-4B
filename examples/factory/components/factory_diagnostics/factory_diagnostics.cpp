/*
 * SPDX-License-Identifier: CC0-1.0
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_hosted.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "others/snapshot/lv_snapshot.h"

#include "bsp/esp-bsp.h"
#include "c6_updater.hpp"
#include "esp_brookesia.hpp"
#include "factory_diagnostics.hpp"
#include "gui_music/lv_demo_music_main.h"

namespace {

constexpr uart_port_t kUart = UART_NUM_0;
constexpr size_t kMaxApps = 16;
constexpr size_t kCommandSize = 96;
constexpr size_t kCaptureRawChunk = 192;
constexpr const char *kTag = "factory_diag";
constexpr char kBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

struct diagnostics_state_t {
    esp_brookesia::systems::phone::Phone *phone;
    factory_diagnostics_app_t apps[kMaxApps];
    size_t app_count;
};

diagnostics_state_t s_state{};

void response(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    printf("FACTORY ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

bool command_equals(const char *left, const char *right)
{
    while (*left && *right) {
        if (tolower(static_cast<unsigned char>(*left)) != tolower(static_cast<unsigned char>(*right))) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

void command_status()
{
    esp_chip_info_t chip{};
    esp_chip_info(&chip);
    uint32_t flash_size = 0;
    esp_err_t flash_ret = esp_flash_get_size(nullptr, &flash_size);

    uint32_t width = 0;
    uint32_t height = 0;
    char active_name[48] = "launcher";
    if (bsp_display_lock(pdMS_TO_TICKS(500))) {
        lv_display_t *display = lv_display_get_default();
        if (display) {
            width = lv_display_get_horizontal_resolution(display);
            height = lv_display_get_vertical_resolution(display);
        }
        auto *active = s_state.phone->getManager().getActiveApp();
        if (active) {
            snprintf(active_name, sizeof(active_name), "%s", active->getName());
        }
        bsp_display_unlock();
    }

    response("STATUS model=%d revision=%d cores=%d idf=%s heap=%u psram=%u flash=%u flash_result=%s display=%lux%lu panel=%s panel_identified=%s",
             chip.model, chip.revision, chip.cores, esp_get_idf_version(),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
             static_cast<unsigned>(flash_size), esp_err_to_name(flash_ret),
             static_cast<unsigned long>(width), static_cast<unsigned long>(height),
             bsp_display_get_panel_name(), bsp_display_panel_is_identified() ? "yes" : "no");
    response("ACTIVE_APP name=%s", active_name);
    wifi_ap_record_t access_point{};
    const esp_err_t wifi_ret = esp_wifi_sta_get_ap_info(&access_point);
    esp_netif_ip_info_t ip{};
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    const bool has_ipv4 = netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0;
    response("NETWORK associated=%s ipv4=%s result=%s",
             wifi_ret == ESP_OK ? "yes" : "no", has_ipv4 ? "yes" : "no", esp_err_to_name(wifi_ret));
}

void command_i2c()
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) {
        response("I2C result=not_initialized");
        return;
    }

    unsigned found = 0;
    for (uint16_t address = 0x03; address <= 0x77; ++address) {
        if (i2c_master_probe(bus, address, 20) == ESP_OK) {
            response("I2C address=0x%02x", address);
            ++found;
        }
        vTaskDelay(1);
    }
    response("I2C done=%u", found);
}

void command_c6_version()
{
    esp_hosted_coprocessor_fwver_t version{};
    const esp_err_t result = esp_hosted_get_coprocessor_fwversion(&version);
    response("C6_VERSION result=%s major=%lu minor=%lu patch=%lu revision=%ld prerelease=%ld build=%ld",
             esp_err_to_name(result), static_cast<unsigned long>(version.major1),
             static_cast<unsigned long>(version.minor1), static_cast<unsigned long>(version.patch1),
             static_cast<long>(version.revision), static_cast<long>(version.prerelease),
             static_cast<long>(version.build));
}

void command_touch()
{
    if (!bsp_display_lock(pdMS_TO_TICKS(500))) {
        response("TOUCH result=display_lock_timeout");
        return;
    }

    lv_indev_t *input = bsp_display_get_input_dev();
    if (!input) {
        bsp_display_unlock();
        response("TOUCH result=input_not_initialized");
        return;
    }

    lv_point_t point{};
    lv_indev_get_point(input, &point);
    const lv_indev_state_t state = lv_indev_get_state(input);
    bsp_display_unlock();
    response("TOUCH source=lvgl state=%s x=%ld y=%ld",
             state == LV_INDEV_STATE_PRESSED ? "pressed" : "released",
             static_cast<long>(point.x), static_cast<long>(point.y));
}

void uart_write_capture_line(const char *data, size_t size)
{
    char line[kCaptureRawChunk / 3 * 4 + 7];
    memcpy(line, "FBCAP ", 6);
    memcpy(line + 6, data, size);
    line[size + 6] = '\n';
    uart_write_bytes(kUart, line, size + 7);
    vTaskDelay(pdMS_TO_TICKS(1));
}

void write_base64_capture(const uint8_t *data, size_t size)
{
    char encoded[kCaptureRawChunk / 3 * 4 + 4];
    while (size >= 3) {
        const size_t triples = size / 3 > kCaptureRawChunk / 3 ? kCaptureRawChunk / 3 : size / 3;
        for (size_t i = 0; i < triples; ++i) {
            const uint8_t *p = data + i * 3;
            encoded[i * 4] = kBase64[p[0] >> 2];
            encoded[i * 4 + 1] = kBase64[((p[0] & 0x03) << 4) | (p[1] >> 4)];
            encoded[i * 4 + 2] = kBase64[((p[1] & 0x0f) << 2) | (p[2] >> 6)];
            encoded[i * 4 + 3] = kBase64[p[2] & 0x3f];
        }
        uart_write_capture_line(encoded, triples * 4);
        data += triples * 3;
        size -= triples * 3;
        taskYIELD();
    }

    if (size == 1) {
        encoded[0] = kBase64[data[0] >> 2];
        encoded[1] = kBase64[(data[0] & 0x03) << 4];
        encoded[2] = '=';
        encoded[3] = '=';
        uart_write_capture_line(encoded, 4);
    } else if (size == 2) {
        encoded[0] = kBase64[data[0] >> 2];
        encoded[1] = kBase64[((data[0] & 0x03) << 4) | (data[1] >> 4)];
        encoded[2] = kBase64[(data[1] & 0x0f) << 2];
        encoded[3] = '=';
        uart_write_capture_line(encoded, 4);
    }
}

void command_capture()
{
    if (!bsp_display_lock(pdMS_TO_TICKS(1000))) {
        response("CAPTURE result=display_lock_timeout");
        return;
    }

    lv_obj_t *screen = lv_screen_active();
    lv_draw_buf_t *snapshot = screen ? lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB565) : nullptr;
    bsp_display_unlock();
    if (!snapshot) {
        response("CAPTURE result=snapshot_failed");
        return;
    }

    const uint32_t width = snapshot->header.w;
    const uint32_t height = snapshot->header.h;
    const uint32_t stride = snapshot->header.stride;
    const size_t size = static_cast<size_t>(stride) * height;
    // Flat UI regions compress well. A short transfer avoids overflowing the
    // USB-UART receiver while retaining every RGB565 pixel and stride byte.
    auto *packed = static_cast<uint8_t *>(heap_caps_malloc(size * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!packed) {
        lv_draw_buf_destroy(snapshot);
        response("CAPTURE result=no_memory");
        return;
    }
    size_t packed_size = 0;
    for (size_t offset = 0; offset < size;) {
        const uint8_t low = snapshot->data[offset];
        const uint8_t high = snapshot->data[offset + 1];
        uint16_t count = 1;
        while (count < UINT16_MAX && offset + static_cast<size_t>(count) * 2 < size &&
                snapshot->data[offset + count * 2] == low &&
                snapshot->data[offset + count * 2 + 1] == high) {
            ++count;
        }
        packed[packed_size++] = count & 0xff;
        packed[packed_size++] = count >> 8;
        packed[packed_size++] = low;
        packed[packed_size++] = high;
        offset += static_cast<size_t>(count) * 2;
    }
    response("CAPTURE_BEGIN format=rgb565_rle width=%lu height=%lu stride=%lu bytes=%u encoded_bytes=%u",
             static_cast<unsigned long>(width), static_cast<unsigned long>(height),
             static_cast<unsigned long>(stride), static_cast<unsigned>(size), static_cast<unsigned>(packed_size));
    write_base64_capture(packed, packed_size);
    response("CAPTURE_END bytes=%u", static_cast<unsigned>(size));
    heap_caps_free(packed);
    lv_draw_buf_destroy(snapshot);
}

void command_app(const char *name, bool close = false)
{
    if (!name || !*name) {
        response("APP result=missing_name");
        return;
    }
    if (!bsp_display_lock(pdMS_TO_TICKS(1000))) {
        response("APP name=%s result=display_lock_timeout", name);
        return;
    }

    esp_brookesia::systems::base::App *app = nullptr;
    for (size_t i = 0; i < s_state.app_count; ++i) {
        if (command_equals(name, s_state.apps[i].command_name)) {
            app = s_state.apps[i].app;
            break;
        }
    }

    bool started = false;
    int id = 0;
    if (app && s_state.phone) {
        id = app->getId();
        const esp_brookesia::systems::base::Context::AppEventData event{
            .id = id,
            .type = close ? esp_brookesia::systems::base::Context::AppEventType::STOP :
                            esp_brookesia::systems::base::Context::AppEventType::START,
            .data = nullptr,
        };
        started = s_state.phone->sendAppEvent(&event);
    }
    bsp_display_unlock();
    response("APP name=%s id=%d action=%s result=%s", name, id, close ? "close" : "start",
             started ? "queued" : "not_found_or_failed");
}

void command_music(const char *action)
{
    if (!bsp_display_lock(pdMS_TO_TICKS(1000))) {
        response("MUSIC result=display_lock_timeout");
        return;
    }
    auto *active = s_state.phone->getManager().getActiveApp();
    if (!active || strcmp(active->getName(), "MusicPlayer") != 0 ||
            lv_demo_music_get_track_count() == 0) {
        bsp_display_unlock();
        response("MUSIC result=open_music_first");
        return;
    }
    if (command_equals(action, "play")) {
        lv_demo_music_play(0);
    } else if (command_equals(action, "resume")) {
        lv_demo_music_resume();
    } else if (command_equals(action, "next")) {
        lv_demo_music_album_next(true);
    } else if (command_equals(action, "pause")) {
        lv_demo_music_pause();
    } else if (!command_equals(action, "status")) {
        bsp_display_unlock();
        response("MUSIC result=use_play_pause_resume_next_or_status");
        return;
    }
    const unsigned tracks = lv_demo_music_get_track_count();
    const int state = audio_player_get_state();
    bsp_display_unlock();
    response("MUSIC request=%s tracks=%u state=%d", action, tracks, state);
}

void process_command(char *line)
{
    char *argument = line;
    while (*argument && !isspace(static_cast<unsigned char>(*argument))) {
        ++argument;
    }
    if (*argument) {
        *argument++ = '\0';
        while (isspace(static_cast<unsigned char>(*argument))) {
            ++argument;
        }
    }

    if (command_equals(line, "factory_status")) {
        command_status();
    } else if (command_equals(line, "factory_i2c")) {
        command_i2c();
    } else if (command_equals(line, "factory_c6_version")) {
        command_c6_version();
    } else if (command_equals(line, "factory_c6_update")) {
        c6_updater_receive_and_install(kUart, argument);
    } else if (command_equals(line, "factory_c6_install")) {
        c6_updater_install_bundled(kUart);
    } else if (command_equals(line, "factory_touch")) {
        command_touch();
    } else if (command_equals(line, "factory_capture")) {
        command_capture();
    } else if (command_equals(line, "factory_app")) {
        command_app(argument);
    } else if (command_equals(line, "factory_close")) {
        command_app(argument, true);
    } else if (command_equals(line, "factory_music")) {
        command_music(argument);
    } else if (command_equals(line, "factory_help")) {
        response("COMMANDS factory_status factory_i2c factory_c6_version factory_c6_update <byte_count> <sha256hex> factory_c6_install factory_touch factory_capture factory_app <calculator|draw|spectrum|music|camera|video|settings|xiaozhi|ethernet|relay|rs485> factory_close <app> factory_music <play|pause|resume|next|status>");
    } else if (*line) {
        response("ERROR unknown_command=%s", line);
    }
}

void diagnostics_task(void *)
{
    char line[kCommandSize]{};
    size_t position = 0;
    uint8_t character = 0;
    while (true) {
        const int read = uart_read_bytes(kUart, &character, 1, pdMS_TO_TICKS(100));
        if (read != 1) {
            continue;
        }
        if (character == '\r' || character == '\n') {
            if (position) {
                line[position] = '\0';
                process_command(line);
                position = 0;
            }
        } else if (isprint(character) && position < sizeof(line) - 1) {
            line[position++] = static_cast<char>(character);
        } else if (position == sizeof(line) - 1) {
            position = 0;
            response("ERROR command_too_long");
        }
    }
}

} // namespace

esp_err_t factory_diagnostics_start(
    esp_brookesia::systems::phone::Phone *phone,
    const factory_diagnostics_app_t *apps,
    size_t app_count)
{
    if (!phone || (!apps && app_count) || app_count > kMaxApps) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_state.phone) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Do not configure UART pins or baud rate: sdkconfig owns the console link. */
    esp_err_t ret = uart_driver_install(kUart, 4096, 0, 0, nullptr, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "Install UART receive driver failed: %s", esp_err_to_name(ret));
        return ret;
    }
    s_state.phone = phone;
    s_state.app_count = app_count;
    for (size_t i = 0; i < app_count; ++i) {
        s_state.apps[i] = apps[i];
    }
    if (xTaskCreate(diagnostics_task, "factory_diag", 8192, nullptr, 4, nullptr) != pdPASS) {
        uart_driver_delete(kUart);
        memset(&s_state, 0, sizeof(s_state));
        return ESP_ERR_NO_MEM;
    }
    response("READY commands=factory_help");
    return ESP_OK;
}
