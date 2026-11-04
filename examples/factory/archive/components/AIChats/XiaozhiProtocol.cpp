/*
 * SPDX-FileCopyrightText: 2025 Shenzhen Xinzhi Future Technology Co., Ltd.
 * SPDX-FileCopyrightText: 2025 Project Contributors
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: MIT
 */

#include "XiaozhiProtocol.hpp"

#include <limits.h>
#include <new>
#include <stdlib.h>
#include <string.h>
#include <utility>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

namespace esp_brookesia::apps {

namespace {

constexpr char TAG[] = "XiaozhiProtocol";
constexpr EventBits_t EVENT_HELLO = BIT0;
constexpr EventBits_t EVENT_ERROR = BIT1;
constexpr EventBits_t EVENT_CLOSED = BIT2;
constexpr EventBits_t EVENT_IO_IDLE = BIT3;
constexpr EventBits_t EVENT_CLEANUP_DONE = BIT4;
constexpr uint32_t SEND_TIMEOUT_MS = 2000;
constexpr uint32_t CLOSE_TIMEOUT_MS = 2000;
constexpr int64_t CHANNEL_TIMEOUT_US = 120LL * 1000 * 1000;
constexpr int DEFAULT_SERVER_SAMPLE_RATE = 24000;
constexpr size_t MAX_MESSAGE_SIZE =
    CONFIG_AICHATS_WEBSOCKET_MAX_MESSAGE_SIZE;
constexpr size_t V2_HEADER_SIZE = 16;
constexpr size_t V3_HEADER_SIZE = 4;

uint16_t readBe16(const uint8_t *data)
{
    return static_cast<uint16_t>(
               static_cast<uint16_t>(data[0]) << 8U |
               static_cast<uint16_t>(data[1])
           );
}

uint32_t readBe32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) << 24U |
           static_cast<uint32_t>(data[1]) << 16U |
           static_cast<uint32_t>(data[2]) << 8U |
           static_cast<uint32_t>(data[3]);
}

void writeBe16(uint8_t *data, uint16_t value)
{
    data[0] = static_cast<uint8_t>(value >> 8U);
    data[1] = static_cast<uint8_t>(value);
}

void writeBe32(uint8_t *data, uint32_t value)
{
    data[0] = static_cast<uint8_t>(value >> 24U);
    data[1] = static_cast<uint8_t>(value >> 16U);
    data[2] = static_cast<uint8_t>(value >> 8U);
    data[3] = static_cast<uint8_t>(value);
}

bool isJsonWhitespace(char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

class SemaphoreGuard {
public:
    explicit SemaphoreGuard(SemaphoreHandle_t semaphore):
        _semaphore(semaphore)
    {
        _locked = _semaphore && xSemaphoreTake(_semaphore, portMAX_DELAY) == pdTRUE;
    }

    ~SemaphoreGuard()
    {
        if (_locked) {
            xSemaphoreGive(_semaphore);
        }
    }

    explicit operator bool() const
    {
        return _locked;
    }

private:
    SemaphoreHandle_t _semaphore = nullptr;
    bool _locked = false;
};

} // namespace

class XiaozhiProtocol::Impl {
public:
    Impl(Config config, Callbacks callbacks):
        _config(std::move(config)),
        _callbacks(std::move(callbacks))
    {
        _mutex = xSemaphoreCreateMutex();
        _events = xEventGroupCreate();
        if (_events) {
            xEventGroupSetBits(_events, EVENT_IO_IDLE | EVENT_CLEANUP_DONE);
        }
        _server_sample_rate = DEFAULT_SERVER_SAMPLE_RATE;
        _server_frame_duration_ms = _config.frame_duration_ms;
    }

    ~Impl()
    {
        stop();
        resetReceiveState();
        if (_events) {
            vEventGroupDelete(_events);
            _events = nullptr;
        }
        if (_mutex) {
            vSemaphoreDelete(_mutex);
            _mutex = nullptr;
        }
    }

    bool isReady() const
    {
        return _mutex && _events;
    }

    esp_err_t start()
    {
        esp_err_t ret = validateConfig();
        if (ret != ESP_OK) {
            reportError(ret, "config");
            return ret;
        }
        if (!isReady()) {
            reportError(ESP_ERR_NO_MEM, "synchronization");
            return ESP_ERR_NO_MEM;
        }

        SemaphoreGuard lock(_mutex);
        if (!lock) {
            return ESP_ERR_INVALID_STATE;
        }
        _started = true;
        return ESP_OK;
    }

    esp_err_t stop()
    {
        if (!_mutex) {
            return ESP_OK;
        }
        {
            SemaphoreGuard lock(_mutex);
            if (lock) {
                _started = false;
            }
        }
        return cleanupClient();
    }

    esp_err_t openAudioChannel()
    {
        if (!isReady()) {
            return ESP_ERR_INVALID_STATE;
        }

        bool cleanup_stale_client = false;
        {
            SemaphoreGuard lock(_mutex);
            if (!lock || !_started) {
                return ESP_ERR_INVALID_STATE;
            }
            const bool channel_timed_out =
                _channel_open && channelTimedOutLocked();
            if (_channel_open && !channel_timed_out) {
                return ESP_OK;
            }
            cleanup_stale_client =
                channel_timed_out ||
                (_client && !_connected && !_opening && !_closing);
            if ((_client && !cleanup_stale_client) || _opening || _closing) {
                return ESP_ERR_INVALID_STATE;
            }
        }
        if (cleanup_stale_client) {
            esp_err_t stale_ret = cleanupClient();
            if (stale_ret != ESP_OK) {
                return stale_ret;
            }
        }

        {
            SemaphoreGuard lock(_mutex);
            if (!lock || !_started || _client || _opening || _closing) {
                return ESP_ERR_INVALID_STATE;
            }
            _opening = true;
            _server_sample_rate = DEFAULT_SERVER_SAMPLE_RATE;
            _server_frame_duration_ms = _config.frame_duration_ms;
            _session_id.clear();
            _last_incoming_us = 0;
        }

        resetReceiveState();
        xEventGroupClearBits(_events, EVENT_HELLO | EVENT_ERROR | EVENT_CLOSED);

        esp_websocket_client_config_t ws_config = {};
        ws_config.uri = _config.url.c_str();
        ws_config.user_context = this;
        ws_config.disable_auto_reconnect = true;
        ws_config.network_timeout_ms = static_cast<int>(_config.handshake_timeout_ms);
        ws_config.buffer_size = 4096;
        ws_config.task_stack = 8192;
        ws_config.crt_bundle_attach = esp_crt_bundle_attach;

        esp_websocket_client_handle_t client =
            esp_websocket_client_init(&ws_config);
        if (!client) {
            clearOpening();
            reportError(ESP_ERR_NO_MEM, "websocket_init");
            return ESP_ERR_NO_MEM;
        }

        esp_err_t ret = appendHeaders(client);
        if (ret == ESP_OK) {
            ret = esp_websocket_register_events(
                      client, WEBSOCKET_EVENT_ANY, websocketEventHandler, this
                  );
        }
        if (ret != ESP_OK) {
            esp_websocket_client_destroy(client);
            clearOpening();
            reportError(ret, "websocket_setup");
            return ret;
        }

        bool accept_client = false;
        {
            SemaphoreGuard lock(_mutex);
            if (lock && _started && _opening && !_closing) {
                if (_active_io == 0) {
                    xEventGroupClearBits(_events, EVENT_IO_IDLE);
                }
                ++_active_io;
                _client = client;
                accept_client = true;
            }
        }
        if (!accept_client) {
            esp_websocket_client_destroy(client);
            clearOpening();
            return ESP_ERR_INVALID_STATE;
        }

        ret = esp_websocket_client_start(client);
        endIo();
        if (ret != ESP_OK) {
            reportError(ret, "websocket_start");
            cleanupClient();
            return ret;
        }

        EventBits_t bits = xEventGroupWaitBits(
                               _events,
                               EVENT_HELLO | EVENT_ERROR | EVENT_CLOSED,
                               pdTRUE,
                               pdFALSE,
                               pdMS_TO_TICKS(_config.handshake_timeout_ms)
                           );
        if ((bits & EVENT_HELLO) != 0 && isAudioChannelOpen()) {
            return ESP_OK;
        }

        ret = (bits & (EVENT_ERROR | EVENT_CLOSED)) != 0 ?
              ESP_FAIL : ESP_ERR_TIMEOUT;
        if (ret == ESP_ERR_TIMEOUT) {
            reportError(ret, "hello_timeout");
        }
        cleanupClient();
        return ret;
    }

    esp_err_t closeAudioChannel()
    {
        return cleanupClient();
    }

    bool isAudioChannelOpen() const
    {
        if (!_mutex) {
            return false;
        }
        SemaphoreGuard lock(_mutex);
        return lock && _channel_open && _connected && !_closing &&
               !channelTimedOutLocked();
    }

    esp_err_t sendAudio(const uint8_t *data, size_t len, uint32_t timestamp)
    {
        if (!data || len == 0 || len > MAX_MESSAGE_SIZE || len > INT_MAX) {
            return ESP_ERR_INVALID_ARG;
        }

        size_t packet_size = len;
        if (_config.version == 2) {
            if (len > UINT32_MAX || len > SIZE_MAX - V2_HEADER_SIZE) {
                return ESP_ERR_INVALID_SIZE;
            }
            packet_size += V2_HEADER_SIZE;
        } else if (_config.version == 3) {
            if (len > UINT16_MAX || len > SIZE_MAX - V3_HEADER_SIZE) {
                return ESP_ERR_INVALID_SIZE;
            }
            packet_size += V3_HEADER_SIZE;
        }
        if (packet_size > MAX_MESSAGE_SIZE || packet_size > INT_MAX) {
            return ESP_ERR_INVALID_SIZE;
        }

        auto *packet = static_cast<uint8_t *>(malloc(packet_size));
        if (!packet) {
            return ESP_ERR_NO_MEM;
        }

        if (_config.version == 2) {
            writeBe16(packet, 2);
            writeBe16(packet + 2, 0);
            writeBe32(packet + 4, 0);
            writeBe32(packet + 8, timestamp);
            writeBe32(packet + 12, static_cast<uint32_t>(len));
            memcpy(packet + V2_HEADER_SIZE, data, len);
        } else if (_config.version == 3) {
            packet[0] = 0;
            packet[1] = 0;
            writeBe16(packet + 2, static_cast<uint16_t>(len));
            memcpy(packet + V3_HEADER_SIZE, data, len);
        } else {
            memcpy(packet, data, len);
        }

        esp_err_t ret = sendBinary(packet, packet_size);
        free(packet);
        return ret;
    }

    esp_err_t sendWakeWordDetected(const std::string &wake_word)
    {
        if (wake_word.empty()) {
            return ESP_ERR_INVALID_ARG;
        }
        cJSON *root = createSessionMessage("listen");
        if (!root || !cJSON_AddStringToObject(root, "state", "detect") ||
                !cJSON_AddStringToObject(root, "text", wake_word.c_str())) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        esp_err_t ret = sendJson(root);
        cJSON_Delete(root);
        return ret;
    }

    esp_err_t sendStartListening(ListeningMode mode)
    {
        const char *mode_text = nullptr;
        switch (mode) {
        case ListeningMode::AutoStop:
            mode_text = "auto";
            break;
        case ListeningMode::ManualStop:
            mode_text = "manual";
            break;
        case ListeningMode::Realtime:
            mode_text = "realtime";
            break;
        }

        cJSON *root = createSessionMessage("listen");
        if (!root || !cJSON_AddStringToObject(root, "state", "start") ||
                !cJSON_AddStringToObject(root, "mode", mode_text)) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        esp_err_t ret = sendJson(root);
        cJSON_Delete(root);
        return ret;
    }

    esp_err_t sendStopListening()
    {
        cJSON *root = createSessionMessage("listen");
        if (!root || !cJSON_AddStringToObject(root, "state", "stop")) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        esp_err_t ret = sendJson(root);
        cJSON_Delete(root);
        return ret;
    }

    esp_err_t sendAbortSpeaking(AbortReason reason)
    {
        cJSON *root = createSessionMessage("abort");
        if (!root) {
            return ESP_ERR_NO_MEM;
        }
        if (reason == AbortReason::WakeWordDetected &&
                !cJSON_AddStringToObject(
                    root, "reason", "wake_word_detected"
                )) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        esp_err_t ret = sendJson(root);
        cJSON_Delete(root);
        return ret;
    }

private:
    Config _config;
    Callbacks _callbacks;
    SemaphoreHandle_t _mutex = nullptr;
    EventGroupHandle_t _events = nullptr;
    esp_websocket_client_handle_t _client = nullptr;
    bool _started = false;
    bool _opening = false;
    bool _closing = false;
    bool _connected = false;
    bool _channel_open = false;
    uint32_t _active_io = 0;
    TaskHandle_t _event_task = nullptr;
    TaskHandle_t _cleanup_task = nullptr;
    std::string _session_id;
    int64_t _last_incoming_us = 0;
    int _server_sample_rate = DEFAULT_SERVER_SAMPLE_RATE;
    int _server_frame_duration_ms = 60;

    uint8_t *_receive_buffer = nullptr;
    size_t _receive_size = 0;
    size_t _receive_capacity = 0;
    bool _message_active = false;
    uint8_t _message_opcode = 0;
    bool _frame_active = false;
    uint8_t _frame_opcode = 0;
    size_t _frame_size = 0;
    size_t _frame_received = 0;
    bool _frame_fin = false;

    esp_err_t validateConfig() const
    {
        if (_config.url.empty() || _config.device_id.empty() ||
                _config.client_id.empty()) {
            return ESP_ERR_INVALID_ARG;
        }
        if (_config.url.rfind("ws://", 0) != 0 &&
                _config.url.rfind("wss://", 0) != 0) {
            return ESP_ERR_INVALID_ARG;
        }
        if (_config.version < 1 || _config.version > 3 ||
                _config.sample_rate < 8000 || _config.sample_rate > 48000 ||
                _config.channels < 1 || _config.channels > 2 ||
                _config.frame_duration_ms < 10 ||
                _config.frame_duration_ms > 120 ||
                _config.handshake_timeout_ms == 0 ||
                _config.handshake_timeout_ms > static_cast<uint32_t>(INT_MAX)) {
            return ESP_ERR_INVALID_ARG;
        }
        return ESP_OK;
    }

    esp_err_t appendHeaders(esp_websocket_client_handle_t client)
    {
        std::string authorization = _config.token;
        if (!authorization.empty() && authorization.find(' ') == std::string::npos) {
            authorization.insert(0, "Bearer ");
        }
        std::string version = std::to_string(_config.version);

        esp_err_t ret = ESP_OK;
        if (!authorization.empty()) {
            ret = esp_websocket_client_append_header(
                      client, "Authorization", authorization.c_str()
                  );
        }
        if (ret == ESP_OK) {
            ret = esp_websocket_client_append_header(
                      client, "Protocol-Version", version.c_str()
                  );
        }
        if (ret == ESP_OK) {
            ret = esp_websocket_client_append_header(
                      client, "Device-Id", _config.device_id.c_str()
                  );
        }
        if (ret == ESP_OK) {
            ret = esp_websocket_client_append_header(
                      client, "Client-Id", _config.client_id.c_str()
                  );
        }
        return ret;
    }

    void clearOpening()
    {
        if (!_mutex) {
            return;
        }
        SemaphoreGuard lock(_mutex);
        if (lock) {
            _opening = false;
        }
    }

    bool channelTimedOutLocked() const
    {
        return _last_incoming_us > 0 &&
               esp_timer_get_time() - _last_incoming_us >
                   CHANNEL_TIMEOUT_US;
    }

    esp_err_t cleanupClient()
    {
        if (!_mutex || !_events) {
            return ESP_OK;
        }
        esp_websocket_client_handle_t client = nullptr;
        bool notify_channel_closed = false;
        bool notify_disconnected = false;
        bool wait_for_cleanup = false;
        TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
        {
            SemaphoreGuard lock(_mutex);
            if (!lock) {
                return ESP_ERR_INVALID_STATE;
            }
            if (_event_task && current_task == _event_task) {
                ESP_LOGE(TAG, "WebSocket cleanup cannot run from its event task");
                return ESP_ERR_INVALID_STATE;
            }
            if (_closing) {
                if (_cleanup_task == current_task) {
                    return ESP_OK;
                }
                wait_for_cleanup = true;
            } else {
                _closing = true;
                _cleanup_task = current_task;
                _opening = false;
                client = _client;
                xEventGroupClearBits(_events, EVENT_CLEANUP_DONE);
            }
        }

        if (wait_for_cleanup) {
            xEventGroupWaitBits(
                _events, EVENT_CLEANUP_DONE, pdFALSE, pdTRUE, portMAX_DELAY
            );
            SemaphoreGuard completed(_mutex);
            return completed ? ESP_OK : ESP_ERR_INVALID_STATE;
        }

        if (client && _events) {
            xEventGroupWaitBits(
                _events, EVENT_IO_IDLE, pdFALSE, pdTRUE, portMAX_DELAY
            );
        }

        esp_err_t first_error = ESP_OK;
        if (client) {
            bool task_stopped = false;
            if (esp_websocket_client_is_connected(client)) {
                esp_err_t ret = esp_websocket_client_close(
                                    client, pdMS_TO_TICKS(CLOSE_TIMEOUT_MS)
                                );
                if (ret == ESP_OK) {
                    task_stopped = true;
                } else {
                    first_error = ret;
                }
            }
            if (!task_stopped) {
                esp_err_t ret = esp_websocket_client_stop(client);
                if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE &&
                        ret != ESP_FAIL && first_error == ESP_OK) {
                    first_error = ret;
                }
            }
            esp_err_t ret = esp_websocket_client_destroy(client);
            if (ret != ESP_OK && first_error == ESP_OK) {
                first_error = ret;
            }
        }

        {
            SemaphoreGuard lock(_mutex);
            if (lock) {
                if (_client == client) {
                    _client = nullptr;
                }
                notify_channel_closed = _channel_open;
                notify_disconnected = _connected;
                _channel_open = false;
                _connected = false;
                _opening = false;
                _session_id.clear();
                _last_incoming_us = 0;
                _active_io = 0;
                _event_task = nullptr;
            }
        }

        resetReceiveState();
        if (notify_channel_closed && _callbacks.onAudioChannelClosed) {
            _callbacks.onAudioChannelClosed();
        }
        if (notify_disconnected && _callbacks.onDisconnected) {
            _callbacks.onDisconnected();
        }

        {
            SemaphoreGuard lock(_mutex);
            if (lock) {
                _cleanup_task = nullptr;
                _closing = false;
                xEventGroupSetBits(_events, EVENT_CLEANUP_DONE);
            }
        }
        return first_error;
    }

    esp_err_t beginIo(
        bool require_channel,
        esp_websocket_client_handle_t &client
    )
    {
        SemaphoreGuard lock(_mutex);
        if (!lock || !_client || !_connected || _closing) {
            return ESP_ERR_INVALID_STATE;
        }
        if (require_channel && channelTimedOutLocked()) {
            return ESP_ERR_TIMEOUT;
        }
        if (require_channel && !_channel_open) {
            return ESP_ERR_INVALID_STATE;
        }
        if (_active_io == 0) {
            xEventGroupClearBits(_events, EVENT_IO_IDLE);
        }
        ++_active_io;
        client = _client;
        return ESP_OK;
    }

    void endIo()
    {
        SemaphoreGuard lock(_mutex);
        if (!lock || _active_io == 0) {
            return;
        }
        --_active_io;
        if (_active_io == 0) {
            xEventGroupSetBits(_events, EVENT_IO_IDLE);
        }
    }

    esp_err_t sendBinary(const uint8_t *data, size_t len)
    {
        esp_websocket_client_handle_t client = nullptr;
        esp_err_t ret = beginIo(true, client);
        if (ret != ESP_OK) {
            return ret;
        }
        int sent = esp_websocket_client_send_bin(
                       client,
                       reinterpret_cast<const char *>(data),
                       static_cast<int>(len),
                       pdMS_TO_TICKS(SEND_TIMEOUT_MS)
                   );
        endIo();
        return sent == static_cast<int>(len) ? ESP_OK : ESP_FAIL;
    }

    esp_err_t sendText(const char *data, size_t len, bool require_channel)
    {
        if (!data || len == 0 || len > MAX_MESSAGE_SIZE || len > INT_MAX) {
            return ESP_ERR_INVALID_ARG;
        }
        esp_websocket_client_handle_t client = nullptr;
        esp_err_t ret = beginIo(require_channel, client);
        if (ret != ESP_OK) {
            return ret;
        }
        int sent = esp_websocket_client_send_text(
                       client,
                       data,
                       static_cast<int>(len),
                       pdMS_TO_TICKS(SEND_TIMEOUT_MS)
                   );
        endIo();
        return sent == static_cast<int>(len) ? ESP_OK : ESP_FAIL;
    }

    esp_err_t sendJson(cJSON *root, bool require_channel = true)
    {
        if (!root) {
            return ESP_ERR_INVALID_ARG;
        }
        char *json = cJSON_PrintUnformatted(root);
        if (!json) {
            return ESP_ERR_NO_MEM;
        }
        size_t len = strlen(json);
        esp_err_t ret = sendText(json, len, require_channel);
        cJSON_free(json);
        return ret;
    }

    cJSON *createSessionMessage(const char *type)
    {
        std::string session_id;
        {
            SemaphoreGuard lock(_mutex);
            if (!lock || !_channel_open || _session_id.empty() || _closing) {
                return nullptr;
            }
            session_id = _session_id;
        }

        cJSON *root = cJSON_CreateObject();
        if (!root ||
                !cJSON_AddStringToObject(root, "session_id", session_id.c_str()) ||
                !cJSON_AddStringToObject(root, "type", type)) {
            cJSON_Delete(root);
            return nullptr;
        }
        return root;
    }

    esp_err_t sendHello()
    {
        cJSON *root = cJSON_CreateObject();
        if (!root) {
            return ESP_ERR_NO_MEM;
        }
        cJSON *features = cJSON_AddObjectToObject(root, "features");
        cJSON *audio = cJSON_AddObjectToObject(root, "audio_params");
        if (!features || !audio ||
                !cJSON_AddBoolToObject(features, "mcp", true) ||
                !cJSON_AddStringToObject(root, "type", "hello") ||
                !cJSON_AddNumberToObject(root, "version", _config.version) ||
                !cJSON_AddStringToObject(root, "transport", "websocket") ||
                !cJSON_AddStringToObject(audio, "format", "opus") ||
                !cJSON_AddNumberToObject(audio, "sample_rate", _config.sample_rate) ||
                !cJSON_AddNumberToObject(audio, "channels", _config.channels) ||
                !cJSON_AddNumberToObject(
                    audio, "frame_duration", _config.frame_duration_ms
                )) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }

        esp_err_t ret = sendJson(root, false);
        cJSON_Delete(root);
        return ret;
    }

    esp_err_t sendMcpPayload(cJSON *payload)
    {
        if (!payload) {
            return ESP_ERR_INVALID_ARG;
        }
        cJSON *root = createSessionMessage("mcp");
        if (!root) {
            cJSON_Delete(payload);
            return ESP_ERR_NO_MEM;
        }
        if (!cJSON_AddItemToObject(root, "payload", payload)) {
            cJSON_Delete(payload);
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }

        esp_err_t ret = sendJson(root);
        cJSON_Delete(root);
        return ret;
    }

    esp_err_t sendMcpReply(
        const cJSON *id,
        const char *field,
        cJSON *value
    )
    {
        cJSON *payload = cJSON_CreateObject();
        cJSON *id_copy = id ? cJSON_Duplicate(id, true) : nullptr;
        if (!payload || !id_copy || !field || !value ||
                !cJSON_AddStringToObject(payload, "jsonrpc", "2.0")) {
            cJSON_Delete(payload);
            cJSON_Delete(id_copy);
            cJSON_Delete(value);
            return ESP_ERR_NO_MEM;
        }
        if (!cJSON_AddItemToObject(payload, "id", id_copy)) {
            cJSON_Delete(payload);
            cJSON_Delete(id_copy);
            cJSON_Delete(value);
            return ESP_ERR_NO_MEM;
        }
        if (!cJSON_AddItemToObject(payload, field, value)) {
            cJSON_Delete(payload);
            cJSON_Delete(value);
            return ESP_ERR_NO_MEM;
        }
        return sendMcpPayload(payload);
    }

    esp_err_t handleMcp(cJSON *root)
    {
        cJSON *payload = cJSON_GetObjectItemCaseSensitive(root, "payload");
        cJSON *version = payload ?
            cJSON_GetObjectItemCaseSensitive(payload, "jsonrpc") : nullptr;
        cJSON *method = payload ?
            cJSON_GetObjectItemCaseSensitive(payload, "method") : nullptr;
        if (!cJSON_IsObject(payload) || !cJSON_IsString(version) ||
                !version->valuestring ||
                strcmp(version->valuestring, "2.0") != 0 ||
                !cJSON_IsString(method) || !method->valuestring) {
            ESP_LOGW(TAG, "Ignoring malformed MCP message");
            return ESP_OK;
        }

        ESP_LOGI(TAG, "Received MCP method: %s", method->valuestring);
        if (strncmp(method->valuestring, "notifications/", 14) == 0) {
            return ESP_OK;
        }

        cJSON *id = cJSON_GetObjectItemCaseSensitive(payload, "id");
        if (!cJSON_IsNumber(id) && !cJSON_IsString(id)) {
            ESP_LOGW(TAG, "Ignoring MCP request without a valid id");
            return ESP_OK;
        }

        if (strcmp(method->valuestring, "initialize") == 0) {
            cJSON *result = cJSON_CreateObject();
            cJSON *capabilities = result ?
                cJSON_AddObjectToObject(result, "capabilities") : nullptr;
            cJSON *tools = capabilities ?
                cJSON_AddObjectToObject(capabilities, "tools") : nullptr;
            cJSON *server_info = result ?
                cJSON_AddObjectToObject(result, "serverInfo") : nullptr;
            const esp_app_desc_t *app = esp_app_get_description();
            if (!result || !capabilities || !tools || !server_info ||
                    !cJSON_AddStringToObject(
                        result, "protocolVersion", "2024-11-05"
                    ) ||
                    !cJSON_AddStringToObject(
                        server_info,
                        "name",
                        "Waveshare ESP32-P4-WIFI6-Touch-LCD-4B"
                    ) ||
                    !cJSON_AddStringToObject(
                        server_info,
                        "version",
                        app && app->version[0] ? app->version : "1"
                    )) {
                cJSON_Delete(result);
                return ESP_ERR_NO_MEM;
            }
            return sendMcpReply(id, "result", result);
        }

        if (strcmp(method->valuestring, "tools/list") == 0) {
            cJSON *result = cJSON_CreateObject();
            cJSON *tools = result ?
                cJSON_AddArrayToObject(result, "tools") : nullptr;
            if (!result || !tools) {
                cJSON_Delete(result);
                return ESP_ERR_NO_MEM;
            }
            return sendMcpReply(id, "result", result);
        }

        cJSON *error = cJSON_CreateObject();
        if (!error ||
                !cJSON_AddNumberToObject(error, "code", -32601) ||
                !cJSON_AddStringToObject(
                    error, "message", "Method not implemented"
                )) {
            cJSON_Delete(error);
            return ESP_ERR_NO_MEM;
        }
        return sendMcpReply(id, "error", error);
    }

    void reportError(esp_err_t error, const char *source)
    {
        if (_events) {
            xEventGroupSetBits(_events, EVENT_ERROR);
        }
        ESP_LOGE(
            TAG, "%s: %s", source ? source : "protocol", esp_err_to_name(error)
        );
        if (_callbacks.onError) {
            _callbacks.onError(error, source ? source : "protocol");
        }
    }

    void notifyConnected()
    {
        bool notify = false;
        {
            SemaphoreGuard lock(_mutex);
            if (lock && !_closing && !_connected) {
                _connected = true;
                notify = true;
            }
        }
        if (notify && _callbacks.onConnected) {
            _callbacks.onConnected();
        }
    }

    void notifyDisconnected()
    {
        bool notify_channel = false;
        bool notify_transport = false;
        {
            SemaphoreGuard lock(_mutex);
            if (lock) {
                notify_channel = _channel_open;
                notify_transport = _connected;
                _channel_open = false;
                _connected = false;
                _opening = false;
                _session_id.clear();
                _last_incoming_us = 0;
            }
        }
        resetReceiveState();
        if (_events) {
            xEventGroupSetBits(_events, EVENT_CLOSED);
        }
        if (notify_channel && _callbacks.onAudioChannelClosed) {
            _callbacks.onAudioChannelClosed();
        }
        if (notify_transport && _callbacks.onDisconnected) {
            _callbacks.onDisconnected();
        }
    }

    static void websocketEventHandler(
        void *handler_arg,
        esp_event_base_t event_base,
        int32_t event_id,
        void *event_data
    )
    {
        (void)event_base;
        auto *instance = static_cast<Impl *>(handler_arg);
        if (instance) {
            instance->handleWebsocketEvent(
                static_cast<esp_websocket_event_id_t>(event_id),
                static_cast<esp_websocket_event_data_t *>(event_data)
            );
        }
    }

    void handleWebsocketEvent(
        esp_websocket_event_id_t event_id,
        esp_websocket_event_data_t *event
    )
    {
        {
            SemaphoreGuard lock(_mutex);
            if (lock && !_event_task) {
                _event_task = xTaskGetCurrentTaskHandle();
            }
        }

        switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED: {
            notifyConnected();
            esp_err_t ret = sendHello();
            if (ret != ESP_OK) {
                reportError(ret, "send_hello");
            }
            break;
        }
        case WEBSOCKET_EVENT_DISCONNECTED:
        case WEBSOCKET_EVENT_CLOSED:
            notifyDisconnected();
            break;
        case WEBSOCKET_EVENT_ERROR: {
            esp_err_t error = ESP_FAIL;
            const char *source = "websocket";
            if (event) {
                switch (event->error_handle.error_type) {
                case WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT:
                    source = "websocket_transport";
                    if (event->error_handle.esp_tls_last_esp_err != ESP_OK) {
                        error = event->error_handle.esp_tls_last_esp_err;
                    }
                    break;
                case WEBSOCKET_ERROR_TYPE_PONG_TIMEOUT:
                    source = "websocket_pong_timeout";
                    error = ESP_ERR_TIMEOUT;
                    break;
                case WEBSOCKET_ERROR_TYPE_HANDSHAKE:
                    source = "websocket_handshake";
                    break;
                case WEBSOCKET_ERROR_TYPE_SERVER_CLOSE:
                    source = "websocket_server_close";
                    break;
                default:
                    break;
                }
            }
            reportError(error, source);
            break;
        }
        case WEBSOCKET_EVENT_DATA:
            if (event && event->op_code < 0x08) {
                esp_err_t ret = acceptFragment(*event);
                if (ret != ESP_OK) {
                    resetReceiveState();
                    reportError(ret, "websocket_frame");
                } else {
                    SemaphoreGuard lock(_mutex);
                    if (lock) {
                        _last_incoming_us = esp_timer_get_time();
                    }
                }
            }
            break;
        default:
            break;
        }
    }

    esp_err_t acceptFragment(const esp_websocket_event_data_t &event)
    {
        if (event.data_len < 0 || event.payload_len < 0 ||
                event.payload_offset < 0 ||
                (event.data_len > 0 && !event.data_ptr)) {
            return ESP_ERR_INVALID_RESPONSE;
        }

        const size_t data_len = static_cast<size_t>(event.data_len);
        const size_t offset = static_cast<size_t>(event.payload_offset);
        const size_t frame_size = event.payload_len > 0 ?
                                  static_cast<size_t>(event.payload_len) : data_len;
        if (frame_size > MAX_MESSAGE_SIZE || offset > frame_size ||
                data_len > frame_size - offset) {
            return ESP_ERR_INVALID_SIZE;
        }

        if (offset == 0) {
            if (_frame_active && _frame_received != _frame_size) {
                return ESP_ERR_INVALID_RESPONSE;
            }
            _frame_active = true;
            _frame_opcode = event.op_code;
            _frame_size = frame_size;
            _frame_received = 0;
            _frame_fin = event.fin;

            if (event.op_code == 0x01 || event.op_code == 0x02) {
                if (_message_active) {
                    return ESP_ERR_INVALID_RESPONSE;
                }
                _message_active = true;
                _message_opcode = event.op_code;
                _receive_size = 0;
            } else if (event.op_code == 0x00) {
                if (!_message_active) {
                    return ESP_ERR_INVALID_RESPONSE;
                }
            } else {
                return ESP_ERR_NOT_SUPPORTED;
            }
        } else if (!_frame_active || offset != _frame_received ||
                   frame_size != _frame_size || event.op_code != _frame_opcode ||
                   event.fin != _frame_fin) {
            return ESP_ERR_INVALID_RESPONSE;
        }

        if (_receive_size > MAX_MESSAGE_SIZE - data_len) {
            return ESP_ERR_INVALID_SIZE;
        }
        esp_err_t ret = ensureReceiveCapacity(_receive_size + data_len);
        if (ret != ESP_OK) {
            return ret;
        }
        if (data_len > 0) {
            memcpy(_receive_buffer + _receive_size, event.data_ptr, data_len);
            _receive_size += data_len;
        }
        _frame_received += data_len;
        if (_frame_received != _frame_size) {
            return ESP_OK;
        }

        _frame_active = false;
        if (!_frame_fin) {
            return ESP_OK;
        }
        if (!_message_active) {
            return ESP_ERR_INVALID_RESPONSE;
        }

        const uint8_t opcode = _message_opcode;
        _message_active = false;
        if (opcode == 0x01) {
            return handleJson(_receive_buffer, _receive_size);
        }
        if (opcode == 0x02) {
            return handleBinary(_receive_buffer, _receive_size);
        }
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_err_t ensureReceiveCapacity(size_t required)
    {
        if (required > MAX_MESSAGE_SIZE) {
            return ESP_ERR_INVALID_SIZE;
        }
        if (required <= _receive_capacity) {
            return ESP_OK;
        }

        size_t capacity = _receive_capacity ? _receive_capacity : 1024;
        while (capacity < required) {
            if (capacity > MAX_MESSAGE_SIZE / 2) {
                capacity = MAX_MESSAGE_SIZE;
                break;
            }
            capacity *= 2;
        }
        auto *buffer = static_cast<uint8_t *>(
                           realloc(_receive_buffer, capacity)
                       );
        if (!buffer) {
            return ESP_ERR_NO_MEM;
        }
        _receive_buffer = buffer;
        _receive_capacity = capacity;
        return ESP_OK;
    }

    void resetReceiveState()
    {
        free(_receive_buffer);
        _receive_buffer = nullptr;
        _receive_size = 0;
        _receive_capacity = 0;
        _message_active = false;
        _message_opcode = 0;
        _frame_active = false;
        _frame_opcode = 0;
        _frame_size = 0;
        _frame_received = 0;
        _frame_fin = false;
    }

    esp_err_t handleBinary(const uint8_t *data, size_t len)
    {
        if ((!data && len > 0) || len > MAX_MESSAGE_SIZE) {
            return ESP_ERR_INVALID_ARG;
        }

        const uint8_t *payload = data;
        size_t payload_len = len;
        uint32_t timestamp = 0;
        uint16_t packet_type = 0;
        if (_config.version == 2) {
            if (len < V2_HEADER_SIZE || readBe16(data) != 2 ||
                    readBe32(data + 4) != 0) {
                return ESP_ERR_INVALID_RESPONSE;
            }
            packet_type = readBe16(data + 2);
            timestamp = readBe32(data + 8);
            uint32_t declared = readBe32(data + 12);
            if (declared != len - V2_HEADER_SIZE) {
                return ESP_ERR_INVALID_SIZE;
            }
            payload = data + V2_HEADER_SIZE;
            payload_len = declared;
        } else if (_config.version == 3) {
            if (len < V3_HEADER_SIZE || data[1] != 0) {
                return ESP_ERR_INVALID_RESPONSE;
            }
            packet_type = data[0];
            uint16_t declared = readBe16(data + 2);
            if (declared != len - V3_HEADER_SIZE) {
                return ESP_ERR_INVALID_SIZE;
            }
            payload = data + V3_HEADER_SIZE;
            payload_len = declared;
        }

        if (packet_type == 1) {
            return handleJson(payload, payload_len);
        }
        if (packet_type != 0 || payload_len == 0) {
            return ESP_ERR_INVALID_RESPONSE;
        }

        int sample_rate = 0;
        int frame_duration = 0;
        {
            SemaphoreGuard lock(_mutex);
            if (!lock || !_channel_open || _closing) {
                return ESP_ERR_INVALID_STATE;
            }
            sample_rate = _server_sample_rate;
            frame_duration = _server_frame_duration_ms;
        }
        if (_callbacks.onAudio) {
            _callbacks.onAudio(
                payload, payload_len, sample_rate, frame_duration, timestamp
            );
        }
        return ESP_OK;
    }

    esp_err_t handleJson(const uint8_t *data, size_t len)
    {
        if (!data || len == 0 || len > MAX_MESSAGE_SIZE) {
            return ESP_ERR_INVALID_ARG;
        }
        const char *parse_end = nullptr;
        cJSON *root = cJSON_ParseWithLengthOpts(
                          reinterpret_cast<const char *>(data),
                          len,
                          &parse_end,
                          false
                      );
        if (!root || !cJSON_IsObject(root)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_RESPONSE;
        }
        const char *limit = reinterpret_cast<const char *>(data) + len;
        while (parse_end && parse_end < limit && isJsonWhitespace(*parse_end)) {
            ++parse_end;
        }
        if (!parse_end || parse_end != limit) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_RESPONSE;
        }

        cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
        if (!cJSON_IsString(type) || !type->valuestring) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_RESPONSE;
        }

        ESP_LOGI(TAG, "Received message type: %s", type->valuestring);
        esp_err_t ret = ESP_OK;
        if (strcmp(type->valuestring, "hello") == 0) {
            ret = handleHello(root);
        } else if (strcmp(type->valuestring, "stt") == 0) {
            cJSON *text = cJSON_GetObjectItemCaseSensitive(root, "text");
            if (!cJSON_IsString(text) || !text->valuestring) {
                ret = ESP_ERR_INVALID_RESPONSE;
            } else {
                ESP_LOGI(TAG, "STT: %s", text->valuestring);
                if (_callbacks.onText) {
                    _callbacks.onText(TextRole::User, text->valuestring);
                }
            }
        } else if (strcmp(type->valuestring, "tts") == 0) {
            ret = handleTts(root);
        } else if (strcmp(type->valuestring, "mcp") == 0) {
            ret = handleMcp(root);
        } else if (strcmp(type->valuestring, "llm") == 0) {
            cJSON *emotion = cJSON_GetObjectItemCaseSensitive(root, "emotion");
            if (!cJSON_IsString(emotion) || !emotion->valuestring) {
                ret = ESP_ERR_INVALID_RESPONSE;
            } else if (_callbacks.onEmotion) {
                _callbacks.onEmotion(emotion->valuestring);
            }
        } else if (strcmp(type->valuestring, "goodbye") == 0) {
            ret = handleGoodbye(root);
        } else {
            ESP_LOGD(TAG, "Ignoring unsupported message type: %s", type->valuestring);
        }
        cJSON_Delete(root);
        return ret;
    }

    esp_err_t handleHello(cJSON *root)
    {
        cJSON *transport = cJSON_GetObjectItemCaseSensitive(root, "transport");
        cJSON *session = cJSON_GetObjectItemCaseSensitive(root, "session_id");
        if (!cJSON_IsString(transport) || !transport->valuestring ||
                strcmp(transport->valuestring, "websocket") != 0 ||
                !cJSON_IsString(session) || !session->valuestring ||
                session->valuestring[0] == '\0') {
            return ESP_ERR_INVALID_RESPONSE;
        }

        int sample_rate = DEFAULT_SERVER_SAMPLE_RATE;
        int frame_duration = _config.frame_duration_ms;
        cJSON *audio = cJSON_GetObjectItemCaseSensitive(root, "audio_params");
        if (audio) {
            if (!cJSON_IsObject(audio)) {
                return ESP_ERR_INVALID_RESPONSE;
            }
            cJSON *rate = cJSON_GetObjectItemCaseSensitive(audio, "sample_rate");
            cJSON *duration =
                cJSON_GetObjectItemCaseSensitive(audio, "frame_duration");
            if (rate) {
                if (!cJSON_IsNumber(rate) || rate->valueint < 8000 ||
                        rate->valueint > 48000) {
                    return ESP_ERR_INVALID_RESPONSE;
                }
                sample_rate = rate->valueint;
            }
            if (duration) {
                if (!cJSON_IsNumber(duration) || duration->valueint < 10 ||
                        duration->valueint > 120) {
                    return ESP_ERR_INVALID_RESPONSE;
                }
                frame_duration = duration->valueint;
            }
        }

        bool notify = false;
        {
            SemaphoreGuard lock(_mutex);
            if (!lock || !_connected || _closing || _channel_open) {
                return ESP_ERR_INVALID_STATE;
            }
            _session_id = session->valuestring;
            _server_sample_rate = sample_rate;
            _server_frame_duration_ms = frame_duration;
            _last_incoming_us = esp_timer_get_time();
            _channel_open = true;
            _opening = false;
            notify = true;
        }
        ESP_LOGI(
            TAG,
            "Server hello accepted (downlink=%d Hz, frame=%d ms)",
            sample_rate, frame_duration
        );
        xEventGroupSetBits(_events, EVENT_HELLO);
        if (notify && _callbacks.onAudioChannelOpened) {
            _callbacks.onAudioChannelOpened();
        }
        return ESP_OK;
    }

    esp_err_t handleTts(cJSON *root)
    {
        cJSON *state = cJSON_GetObjectItemCaseSensitive(root, "state");
        if (!cJSON_IsString(state) || !state->valuestring) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        const std::string empty;
        if (strcmp(state->valuestring, "start") == 0) {
            if (_callbacks.onTts) {
                _callbacks.onTts(TtsState::Start, empty);
            }
            return ESP_OK;
        }
        if (strcmp(state->valuestring, "stop") == 0) {
            if (_callbacks.onTts) {
                _callbacks.onTts(TtsState::Stop, empty);
            }
            return ESP_OK;
        }
        if (strcmp(state->valuestring, "sentence_start") == 0) {
            cJSON *text = cJSON_GetObjectItemCaseSensitive(root, "text");
            if (!cJSON_IsString(text) || !text->valuestring) {
                return ESP_ERR_INVALID_RESPONSE;
            }
            std::string sentence(text->valuestring);
            if (_callbacks.onTts) {
                _callbacks.onTts(TtsState::SentenceStart, sentence);
            }
            if (_callbacks.onText) {
                _callbacks.onText(TextRole::Assistant, sentence);
            }
            return ESP_OK;
        }
        ESP_LOGW(
            TAG, "Ignoring unknown TTS state: %s",
            state->valuestring
        );
        return ESP_OK;
    }

    esp_err_t handleGoodbye(cJSON *root)
    {
        cJSON *session = cJSON_GetObjectItemCaseSensitive(root, "session_id");
        if (session && (!cJSON_IsString(session) || !session->valuestring)) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        if (session) {
            SemaphoreGuard lock(_mutex);
            if (!lock || _session_id != session->valuestring) {
                return ESP_OK;
            }
        }
        if (_callbacks.onServerGoodbye) {
            _callbacks.onServerGoodbye();
        }
        return ESP_OK;
    }
};

XiaozhiProtocol::Config::Config(
    std::string url_value,
    std::string token_value,
    int version_value,
    std::string device_id_value,
    std::string client_id_value,
    int sample_rate_value,
    int channels_value,
    int frame_duration_ms_value,
    uint32_t handshake_timeout_ms_value
):
    url(std::move(url_value)),
    token(std::move(token_value)),
    version(version_value),
    device_id(std::move(device_id_value)),
    client_id(std::move(client_id_value)),
    sample_rate(sample_rate_value),
    channels(channels_value),
    frame_duration_ms(frame_duration_ms_value),
    handshake_timeout_ms(handshake_timeout_ms_value)
{
}

XiaozhiProtocol::XiaozhiProtocol(Config config, Callbacks callbacks)
{
    _impl = new (std::nothrow) Impl(std::move(config), std::move(callbacks));
}

XiaozhiProtocol::~XiaozhiProtocol()
{
    delete _impl;
    _impl = nullptr;
}

esp_err_t XiaozhiProtocol::start()
{
    return _impl ? _impl->start() : ESP_ERR_NO_MEM;
}

esp_err_t XiaozhiProtocol::stop()
{
    return _impl ? _impl->stop() : ESP_OK;
}

esp_err_t XiaozhiProtocol::openAudioChannel()
{
    return _impl ? _impl->openAudioChannel() : ESP_ERR_NO_MEM;
}

esp_err_t XiaozhiProtocol::closeAudioChannel()
{
    return _impl ? _impl->closeAudioChannel() : ESP_OK;
}

bool XiaozhiProtocol::isAudioChannelOpen() const
{
    return _impl && _impl->isAudioChannelOpen();
}

esp_err_t XiaozhiProtocol::sendAudio(
    const uint8_t *data,
    size_t len,
    uint32_t timestamp
)
{
    return _impl ? _impl->sendAudio(data, len, timestamp) : ESP_ERR_NO_MEM;
}

esp_err_t XiaozhiProtocol::sendWakeWordDetected(const std::string &wake_word)
{
    return _impl ?
           _impl->sendWakeWordDetected(wake_word) : ESP_ERR_NO_MEM;
}

esp_err_t XiaozhiProtocol::sendStartListening(ListeningMode mode)
{
    return _impl ? _impl->sendStartListening(mode) : ESP_ERR_NO_MEM;
}

esp_err_t XiaozhiProtocol::sendStopListening()
{
    return _impl ? _impl->sendStopListening() : ESP_ERR_NO_MEM;
}

esp_err_t XiaozhiProtocol::sendAbortSpeaking(AbortReason reason)
{
    return _impl ? _impl->sendAbortSpeaking(reason) : ESP_ERR_NO_MEM;
}

} // namespace esp_brookesia::apps
