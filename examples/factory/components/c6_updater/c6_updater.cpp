#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_hosted.h"
#include "esp_hosted_host_fw_ver.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include "sdkconfig.h"

#include "c6_bundled_hash.hpp"
#include "c6_updater.hpp"

namespace {

constexpr size_t kMaxImageBytes = 1920 * 1024;
constexpr size_t kUploadChunkBytes = 1024;
constexpr size_t kHostedChunkBytes = 1024;
constexpr TickType_t kChunkTimeout = pdMS_TO_TICKS(3000);
constexpr size_t kVersionBytes = sizeof(((esp_app_desc_t *)nullptr)->version);
constexpr char kBundledImagePath[] = CONFIG_BSP_SPIFFS_MOUNT_POINT "/c6/esp_hosted-3.0.10.bin";
constexpr const char *kBundledImageSha256 = FACTORY_C6_BUNDLED_SHA256;

const char *expected_project_name()
{
#if ESP_HOSTED_VERSION_MAJOR_1 == 2
    return "network_adapter";
#elif ESP_HOSTED_VERSION_MAJOR_1 == 3
    return "eh_cp_wifi_sta";
#else
    return nullptr;
#endif
}

void response(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    printf("FACTORY C6_UPDATE ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

bool parse_sha256(const char *text, uint8_t digest[32])
{
    if (!text || strlen(text) != 64) {
        return false;
    }
    for (size_t i = 0; i < 32; ++i) {
        const unsigned char hi = static_cast<unsigned char>(text[i * 2]);
        const unsigned char lo = static_cast<unsigned char>(text[i * 2 + 1]);
        if (!isxdigit(hi) || !isxdigit(lo)) {
            return false;
        }
        const uint8_t high = isdigit(hi) ? hi - '0' : (tolower(hi) - 'a' + 10);
        const uint8_t low = isdigit(lo) ? lo - '0' : (tolower(lo) - 'a' + 10);
        digest[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

bool parse_request(const char *arguments, size_t *image_bytes, uint8_t expected_digest[32])
{
    if (!arguments || !image_bytes || !expected_digest) {
        return false;
    }

    char bytes_text[11]{};
    char digest_text[65]{};
    char extra = 0;
    if (sscanf(arguments, "%10s %64s %c", bytes_text, digest_text, &extra) != 2) {
        return false;
    }
    char *end = nullptr;
    const unsigned long bytes = strtoul(bytes_text, &end, 10);
    if (!*bytes_text || !end || *end != '\0' || bytes == 0 || bytes > kMaxImageBytes) {
        return false;
    }
    *image_bytes = static_cast<size_t>(bytes);
    return parse_sha256(digest_text, expected_digest);
}

bool parse_version_major_minor(const char *version, uint32_t *major, uint32_t *minor)
{
    if (!version || !major || !minor) {
        return false;
    }
    const char *cursor = version;
    uint32_t values[2]{};
    for (size_t field = 0; field < 2; ++field) {
        if (!isdigit(static_cast<unsigned char>(*cursor))) {
            return false;
        }
        do {
            const uint32_t digit = static_cast<uint32_t>(*cursor - '0');
            if (values[field] > (UINT32_MAX - digit) / 10) {
                return false;
            }
            values[field] = values[field] * 10 + digit;
            ++cursor;
        } while (isdigit(static_cast<unsigned char>(*cursor)));
        if (field == 0) {
            if (*cursor++ != '.') {
                return false;
            }
        }
    }
    if (*cursor != '\0' && *cursor != '.') {
        return false;
    }
    *major = values[0];
    *minor = values[1];
    return true;
}

bool validate_image(const uint8_t *image, size_t image_bytes, char version[kVersionBytes + 1])
{
    if (!image || image_bytes < sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
        response("ERROR stage=image_format reason=too_short");
        return false;
    }

    const auto *header = reinterpret_cast<const esp_image_header_t *>(image);
    if (header->magic != ESP_IMAGE_HEADER_MAGIC || header->segment_count == 0 || header->segment_count > ESP_IMAGE_MAX_SEGMENTS) {
        response("ERROR stage=image_format reason=header");
        return false;
    }
    if (header->chip_id != ESP_CHIP_ID_ESP32C6) {
        response("ERROR stage=image_format reason=chip expected=esp32c6 actual=%u", static_cast<unsigned>(header->chip_id));
        return false;
    }

    const auto *segment = reinterpret_cast<const esp_image_segment_header_t *>(image + sizeof(*header));
    if (segment->data_len < sizeof(esp_app_desc_t) ||
            segment->data_len > image_bytes - sizeof(*header) - sizeof(*segment)) {
        response("ERROR stage=image_format reason=first_segment");
        return false;
    }
    const auto *descriptor = reinterpret_cast<const esp_app_desc_t *>(image + sizeof(*header) + sizeof(*segment));
    if (descriptor->magic_word != ESP_APP_DESC_MAGIC_WORD) {
        response("ERROR stage=image_format reason=app_descriptor");
        return false;
    }

    const char *expected_project = expected_project_name();
    if (!expected_project) {
        response("ERROR stage=image_format reason=unsupported_host_major actual=%u", ESP_HOSTED_VERSION_MAJOR_1);
        return false;
    }
    char project[sizeof(descriptor->project_name) + 1]{};
    memcpy(project, descriptor->project_name, sizeof(descriptor->project_name));
    if (strcmp(project, expected_project) != 0) {
        response("ERROR stage=image_format reason=project expected=%s actual=%s", expected_project, project);
        return false;
    }

    memcpy(version, descriptor->version, sizeof(descriptor->version));
    version[sizeof(descriptor->version)] = '\0';
    uint32_t major = 0;
    uint32_t minor = 0;
    if (!parse_version_major_minor(version, &major, &minor) ||
            major != ESP_HOSTED_VERSION_MAJOR_1 || minor != ESP_HOSTED_VERSION_MINOR_1) {
        response("ERROR stage=image_format reason=hosted_version expected=%u.%u actual=%s",
                 ESP_HOSTED_VERSION_MAJOR_1, ESP_HOSTED_VERSION_MINOR_1, version);
        return false;
    }
    return true;
}

bool receive_image(uart_port_t uart, uint8_t *image, size_t image_bytes)
{
    response("READY bytes=%u chunk=%u", static_cast<unsigned>(image_bytes), static_cast<unsigned>(kUploadChunkBytes));
    for (size_t offset = 0; offset < image_bytes;) {
        const size_t expected = (image_bytes - offset > kUploadChunkBytes) ? kUploadChunkBytes : image_bytes - offset;
        size_t received = 0;
        while (received < expected) {
            const int count = uart_read_bytes(uart, image + offset + received, expected - received, kChunkTimeout);
            if (count <= 0) {
                response("ERROR stage=upload_timeout offset=%u expected=%u received=%u",
                         static_cast<unsigned>(offset), static_cast<unsigned>(expected), static_cast<unsigned>(received));
                return false;
            }
            received += static_cast<size_t>(count);
        }
        offset += expected;
        response("ACK offset=%u", static_cast<unsigned>(offset));
    }
    return true;
}

bool digest_matches(const uint8_t *image, size_t image_bytes, const uint8_t expected_digest[32])
{
    uint8_t actual_digest[32]{};
    if (mbedtls_sha256(image, image_bytes, actual_digest, 0) != 0) {
        response("ERROR stage=sha256 reason=calculation_failed");
        return false;
    }
    if (memcmp(actual_digest, expected_digest, sizeof(actual_digest)) != 0) {
        response("ERROR stage=sha256 reason=mismatch");
        return false;
    }
    return true;
}

bool load_bundled_image(uint8_t **image_out, size_t *image_bytes)
{
    FILE *file = fopen(kBundledImagePath, "rb");
    if (!file) {
        response("ERROR stage=bundled_open path=%s", kBundledImagePath);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        response("ERROR stage=bundled_size path=%s", kBundledImagePath);
        return false;
    }
    const long file_size = ftell(file);
    if (file_size <= 0 || static_cast<unsigned long>(file_size) > kMaxImageBytes || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        response("ERROR stage=bundled_size path=%s", kBundledImagePath);
        return false;
    }

    const size_t bytes = static_cast<size_t>(file_size);
    auto *image = static_cast<uint8_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!image) {
        fclose(file);
        response("ERROR stage=allocate bytes=%u", static_cast<unsigned>(bytes));
        return false;
    }
    size_t offset = 0;
    while (offset < bytes) {
        const size_t count = fread(image + offset, 1, bytes - offset, file);
        if (count == 0) {
            heap_caps_free(image);
            fclose(file);
            response("ERROR stage=bundled_read offset=%u", static_cast<unsigned>(offset));
            return false;
        }
        offset += count;
    }
    fclose(file);
    *image_out = image;
    *image_bytes = bytes;
    return true;
}

bool preupdate_version_supports_activate()
{
    esp_hosted_coprocessor_fwver_t version{};
    const esp_err_t result = esp_hosted_get_coprocessor_fwversion(&version);
    if (result != ESP_OK) {
        response("INFO preupdate_version=unknown result=%s activation=legacy_end", esp_err_to_name(result));
        return false;
    }
    if (version.major1 == 0 && version.minor1 == 0 && version.patch1 == 0) {
        response("INFO preupdate_version=unknown result=zero_version activation=legacy_end");
        return false;
    }
    const bool supported = version.major1 > 2 || (version.major1 == 2 && version.minor1 >= 6);
    response("INFO preupdate_version=%lu.%lu.%lu activation=%s",
             static_cast<unsigned long>(version.major1), static_cast<unsigned long>(version.minor1),
             static_cast<unsigned long>(version.patch1), supported ? "required" : "legacy_end");
    return supported;
}

bool write_image_to_c6(const uint8_t *image, size_t image_bytes)
{
    const bool activate_required = preupdate_version_supports_activate();
    esp_err_t result = esp_hosted_slave_ota_begin();
    if (result != ESP_OK) {
        response("ERROR stage=ota_begin result=%s", esp_err_to_name(result));
        return false;
    }

    for (size_t offset = 0; offset < image_bytes; offset += kHostedChunkBytes) {
        const size_t bytes = (image_bytes - offset > kHostedChunkBytes) ? kHostedChunkBytes : image_bytes - offset;
        result = esp_hosted_slave_ota_write(const_cast<uint8_t *>(image + offset), bytes);
        if (result != ESP_OK) {
            response("ERROR stage=ota_write offset=%u result=%s", static_cast<unsigned>(offset), esp_err_to_name(result));
            return false;
        }
    }

    result = esp_hosted_slave_ota_end();
    if (result != ESP_OK) {
        response("ERROR stage=ota_end result=%s", esp_err_to_name(result));
        return false;
    }
    if (activate_required) {
        result = esp_hosted_slave_ota_activate();
        if (result != ESP_OK) {
            response("ERROR stage=ota_activate result=%s", esp_err_to_name(result));
            return false;
        }
    }
    return true;
}

bool validate_and_write_image(const uint8_t *image, size_t image_bytes, const uint8_t expected_digest[32],
                              char version[kVersionBytes + 1])
{
    return digest_matches(image, image_bytes, expected_digest) &&
           validate_image(image, image_bytes, version) &&
           write_image_to_c6(image, image_bytes);
}

void report_success_and_restart(uart_port_t uart, size_t image_bytes, const char *version)
{
    response("RESULT success bytes=%u version=%s rebooting=yes", static_cast<unsigned>(image_bytes), version);
    fflush(stdout);
    uart_wait_tx_done(uart, pdMS_TO_TICKS(2000));
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
}

} // namespace

void c6_updater_receive_and_install(uart_port_t uart, const char *arguments)
{
    size_t image_bytes = 0;
    uint8_t expected_digest[32]{};
    if (!parse_request(arguments, &image_bytes, expected_digest)) {
        response("ERROR stage=request usage=factory_c6_update <byte_count> <sha256hex>");
        return;
    }

    auto *image = static_cast<uint8_t *>(heap_caps_malloc(image_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!image) {
        response("ERROR stage=allocate bytes=%u", static_cast<unsigned>(image_bytes));
        return;
    }

    char version[kVersionBytes + 1]{};
    const bool uploaded = receive_image(uart, image, image_bytes);
    const bool installed = uploaded && validate_and_write_image(image, image_bytes, expected_digest, version);
    heap_caps_free(image);
    if (!installed) {
        return;
    }
    report_success_and_restart(uart, image_bytes, version);
}

void c6_updater_install_bundled(uart_port_t uart)
{
    uint8_t expected_digest[32]{};
    if (!parse_sha256(kBundledImageSha256, expected_digest)) {
        response("ERROR stage=bundled_hash_config");
        return;
    }

    uint8_t *image = nullptr;
    size_t image_bytes = 0;
    if (!load_bundled_image(&image, &image_bytes)) {
        return;
    }

    char version[kVersionBytes + 1]{};
    const bool installed = validate_and_write_image(image, image_bytes, expected_digest, version);
    heap_caps_free(image);
    if (!installed) {
        return;
    }
    report_success_and_restart(uart, image_bytes, version);
}
