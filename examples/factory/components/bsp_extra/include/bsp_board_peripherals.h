/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_EXTRA_RELAY_COUNT             (2)
#define BSP_EXTRA_RELAY_1_GPIO            (32)
#define BSP_EXTRA_RELAY_2_GPIO            (46)

#define BSP_EXTRA_RS485_TX_GPIO           (47)
#define BSP_EXTRA_RS485_RX_GPIO           (48)
#define BSP_EXTRA_RS485_BAUD_RATE          (115200)

typedef struct {
    bool initialized;
    bool started;
    bool link_up;
    bool ip_acquired;
    bool full_duplex;
    uint16_t speed_mbps;
    uint8_t mac[6];
    esp_netif_ip_info_t ip_info;
    esp_err_t last_error;
} bsp_extra_ethernet_info_t;

/** Initialize both active-high relay outputs in the safe OFF state. */
esp_err_t bsp_extra_relay_init(void);

/** Set one relay. Relay index is 0 for relay 1 and 1 for relay 2. */
esp_err_t bsp_extra_relay_set(size_t relay_index, bool enabled);

/** Read the last commanded relay output level. */
bool bsp_extra_relay_get(size_t relay_index);

/** Start the on-board IP101 Ethernet interface and DHCP client. */
esp_err_t bsp_extra_ethernet_init(void);

/** Copy the latest Ethernet link and addressing snapshot. */
esp_err_t bsp_extra_ethernet_get_info(bsp_extra_ethernet_info_t *info);

/** Initialize UART1 for the isolated RS485 interface. */
esp_err_t bsp_extra_rs485_init(void);

/** Write bytes to the RS485 UART. */
esp_err_t bsp_extra_rs485_write(const void *data, size_t size, size_t *bytes_written);

/** Read bytes from the RS485 UART. */
esp_err_t bsp_extra_rs485_read(void *data, size_t size, size_t *bytes_read, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
