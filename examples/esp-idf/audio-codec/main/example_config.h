/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "sdkconfig.h"

#if CONFIG_EXAMPLE_MODE_ECHO
#define EXAMPLE_SAMPLE_RATE       24000
#else
#define EXAMPLE_SAMPLE_RATE       16000
#endif
#define EXAMPLE_CHANNEL_COUNT     2
#define EXAMPLE_BITS_PER_SAMPLE   16
#define EXAMPLE_ECHO_BUFFER_SIZE  2400
#define EXAMPLE_VOICE_VOLUME      CONFIG_EXAMPLE_VOICE_VOLUME
