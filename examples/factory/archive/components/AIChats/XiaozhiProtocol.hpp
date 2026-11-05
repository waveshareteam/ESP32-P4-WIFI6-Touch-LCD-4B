/*
 * SPDX-FileCopyrightText: 2025 Shenzhen Xinzhi Future Technology Co., Ltd.
 * SPDX-FileCopyrightText: 2025 Project Contributors
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <functional>
#include <stddef.h>
#include <stdint.h>
#include <string>

#include "esp_err.h"

namespace esp_brookesia::apps {

/**
 * Lightweight Xiaozhi WebSocket protocol client.
 *
 * The implementation is adapted from the Protocol and WebsocketProtocol
 * classes in xiaozhi-esp32 commit 1b48ebd7863695bf80d384c6c09af6299a6d7d0e.
 * It intentionally owns only protocol and transport behavior; UI, audio
 * capture, Opus coding, and application state remain outside this class.
 */
class XiaozhiProtocol final {
public:
    enum class ListeningMode : uint8_t {
        AutoStop,
        ManualStop,
        Realtime,
    };

    enum class AbortReason : uint8_t {
        None,
        WakeWordDetected,
    };

    enum class TextRole : uint8_t {
        User,
        Assistant,
    };

    enum class TtsState : uint8_t {
        Start,
        Stop,
        SentenceStart,
    };

    struct Config {
        std::string url;
        std::string token;
        int version = 1;
        std::string device_id;
        std::string client_id;
        int sample_rate = 16000;
        int channels = 1;
        int frame_duration_ms = 60;
        uint32_t handshake_timeout_ms = 10000;

        Config() = default;
        Config(
            std::string url,
            std::string token,
            int version,
            std::string device_id,
            std::string client_id,
            int sample_rate = 16000,
            int channels = 1,
            int frame_duration_ms = 60,
            uint32_t handshake_timeout_ms = 10000
        );
    };

    struct Callbacks {
        // Callbacks may run on the WebSocket event task or on the lifecycle
        // caller that reports a setup/cleanup result. They must be thread-safe;
        // lifecycle methods and object destruction must be dispatched elsewhere.
        std::function<void()> onConnected;
        std::function<void()> onDisconnected;
        std::function<void()> onAudioChannelOpened;
        std::function<void()> onAudioChannelClosed;
        std::function<void()> onServerGoodbye;
        std::function<void(TextRole role, const std::string &text)> onText;
        std::function<void(const std::string &emotion)> onEmotion;
        std::function<void(TtsState state, const std::string &text)> onTts;

        // The audio buffer is valid only for the duration of this callback.
        std::function<void(
            const uint8_t *data,
            size_t len,
            int sample_rate,
            int frame_duration_ms,
            uint32_t timestamp
        )> onAudio;
        std::function<void(esp_err_t error, const std::string &source)> onError;
    };

    explicit XiaozhiProtocol(Config config, Callbacks callbacks = {});
    ~XiaozhiProtocol();

    XiaozhiProtocol(const XiaozhiProtocol &) = delete;
    XiaozhiProtocol &operator=(const XiaozhiProtocol &) = delete;
    XiaozhiProtocol(XiaozhiProtocol &&) = delete;
    XiaozhiProtocol &operator=(XiaozhiProtocol &&) = delete;

    esp_err_t start();
    esp_err_t stop();
    esp_err_t openAudioChannel();
    esp_err_t closeAudioChannel();
    bool isAudioChannelOpen() const;

    esp_err_t sendAudio(
        const uint8_t *data,
        size_t len,
        uint32_t timestamp = 0
    );
    esp_err_t sendWakeWordDetected(const std::string &wake_word);
    esp_err_t sendStartListening(ListeningMode mode);
    esp_err_t sendStopListening();
    esp_err_t sendAbortSpeaking(AbortReason reason);

private:
    class Impl;
    Impl *_impl = nullptr;
};

} // namespace esp_brookesia::apps
