/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "bsp_board_peripherals.h"

#define BSP_EXTRA_ETH_PHY_ADDRESS         (1)
#define BSP_EXTRA_ETH_PHY_RESET_GPIO      (51)
#define BSP_EXTRA_ETH_MDC_GPIO            (31)
#define BSP_EXTRA_ETH_MDIO_GPIO           (52)
#define BSP_EXTRA_ETH_RMII_CLOCK_GPIO     (50)
#define BSP_EXTRA_ETH_TX_EN_GPIO          (49)
#define BSP_EXTRA_ETH_TXD0_GPIO           (34)
#define BSP_EXTRA_ETH_TXD1_GPIO           (35)
#define BSP_EXTRA_ETH_CRS_DV_GPIO         (28)
#define BSP_EXTRA_ETH_RXD0_GPIO           (29)
#define BSP_EXTRA_ETH_RXD1_GPIO           (30)

#define BSP_EXTRA_RS485_UART              UART_NUM_1
#define BSP_EXTRA_RS485_RX_BUFFER_SIZE    (2048)
#define BSP_EXTRA_RS485_TX_BUFFER_SIZE    (2048)

static const char *TAG = "bsp_extra_periph";

static bool s_relay_initialized;
static uint8_t s_relay_states;
static portMUX_TYPE s_relay_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_rs485_initialized;
static bool s_ethernet_initializing;
static portMUX_TYPE s_ethernet_lock = portMUX_INITIALIZER_UNLOCKED;
static bsp_extra_ethernet_info_t s_ethernet_info = {
    .last_error = ESP_ERR_INVALID_STATE,
};

static esp_eth_mac_t *s_eth_mac;
static esp_eth_phy_t *s_eth_phy;
static esp_eth_handle_t s_eth_handle;
static esp_netif_t *s_eth_netif;
static esp_eth_netif_glue_handle_t s_eth_glue;
static esp_event_handler_instance_t s_eth_event_instance;
static esp_event_handler_instance_t s_eth_ip_event_instance;

static int relay_gpio(size_t relay_index)
{
    static const int relay_gpios[BSP_EXTRA_RELAY_COUNT] = {
        BSP_EXTRA_RELAY_1_GPIO,
        BSP_EXTRA_RELAY_2_GPIO,
    };

    return relay_index < BSP_EXTRA_RELAY_COUNT ? relay_gpios[relay_index] : GPIO_NUM_NC;
}

esp_err_t bsp_extra_relay_init(void)
{
    if (s_relay_initialized) {
        return ESP_OK;
    }

    gpio_config_t config = {
        .pin_bit_mask = (1ULL << BSP_EXTRA_RELAY_1_GPIO) | (1ULL << BSP_EXTRA_RELAY_2_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "Configure relay GPIOs failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(BSP_EXTRA_RELAY_1_GPIO, 0), TAG, "Disable relay 1 failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(BSP_EXTRA_RELAY_2_GPIO, 0), TAG, "Disable relay 2 failed");

    taskENTER_CRITICAL(&s_relay_lock);
    s_relay_states = 0;
    taskEXIT_CRITICAL(&s_relay_lock);
    s_relay_initialized = true;
    return ESP_OK;
}

esp_err_t bsp_extra_relay_set(size_t relay_index, bool enabled)
{
    int gpio = relay_gpio(relay_index);
    ESP_RETURN_ON_FALSE(
        gpio != GPIO_NUM_NC,
        ESP_ERR_INVALID_ARG,
        TAG,
        "Invalid relay index: %u",
        (unsigned)relay_index
    );
    ESP_RETURN_ON_ERROR(bsp_extra_relay_init(), TAG, "Relay initialization failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(gpio, enabled ? 1 : 0), TAG, "Set relay GPIO failed");

    taskENTER_CRITICAL(&s_relay_lock);
    if (enabled) {
        s_relay_states |= (uint8_t)(1U << relay_index);
    } else {
        s_relay_states &= (uint8_t)~(1U << relay_index);
    }
    taskEXIT_CRITICAL(&s_relay_lock);
    return ESP_OK;
}

bool bsp_extra_relay_get(size_t relay_index)
{
    int gpio = relay_gpio(relay_index);
    if (gpio == GPIO_NUM_NC || !s_relay_initialized) {
        return false;
    }

    taskENTER_CRITICAL(&s_relay_lock);
    bool enabled = (s_relay_states & (uint8_t)(1U << relay_index)) != 0;
    taskEXIT_CRITICAL(&s_relay_lock);
    return enabled;
}

static void ethernet_set_error(esp_err_t error)
{
    taskENTER_CRITICAL(&s_ethernet_lock);
    s_ethernet_info.last_error = error;
    taskEXIT_CRITICAL(&s_ethernet_lock);
}

static void ethernet_refresh_link_details(esp_eth_handle_t eth_handle)
{
    uint8_t mac[6] = {0};
    eth_speed_t speed = ETH_SPEED_10M;
    eth_duplex_t duplex = ETH_DUPLEX_HALF;

    esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac);
    esp_eth_ioctl(eth_handle, ETH_CMD_G_SPEED, &speed);
    esp_eth_ioctl(eth_handle, ETH_CMD_G_DUPLEX_MODE, &duplex);

    taskENTER_CRITICAL(&s_ethernet_lock);
    memcpy(s_ethernet_info.mac, mac, sizeof(mac));
    s_ethernet_info.speed_mbps = speed == ETH_SPEED_100M ? 100 : 10;
    s_ethernet_info.full_duplex = duplex == ETH_DUPLEX_FULL;
    taskEXIT_CRITICAL(&s_ethernet_lock);
}

static void ethernet_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    esp_eth_handle_t eth_handle = event_data ? *(esp_eth_handle_t *)event_data : s_eth_handle;

    taskENTER_CRITICAL(&s_ethernet_lock);
    switch (event_id) {
    case ETHERNET_EVENT_START:
        s_ethernet_info.started = true;
        break;
    case ETHERNET_EVENT_STOP:
        s_ethernet_info.started = false;
        s_ethernet_info.link_up = false;
        s_ethernet_info.ip_acquired = false;
        memset(&s_ethernet_info.ip_info, 0, sizeof(s_ethernet_info.ip_info));
        break;
    case ETHERNET_EVENT_CONNECTED:
        s_ethernet_info.link_up = true;
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        s_ethernet_info.link_up = false;
        s_ethernet_info.ip_acquired = false;
        memset(&s_ethernet_info.ip_info, 0, sizeof(s_ethernet_info.ip_info));
        break;
    default:
        break;
    }
    taskEXIT_CRITICAL(&s_ethernet_lock);

    if (event_id == ETHERNET_EVENT_CONNECTED && eth_handle) {
        ethernet_refresh_link_details(eth_handle);
    }
}

static void ethernet_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    taskENTER_CRITICAL(&s_ethernet_lock);
    if (event_id == IP_EVENT_ETH_GOT_IP && event_data) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        s_ethernet_info.ip_info = event->ip_info;
        s_ethernet_info.ip_acquired = true;
    } else if (event_id == IP_EVENT_ETH_LOST_IP) {
        s_ethernet_info.ip_acquired = false;
        memset(&s_ethernet_info.ip_info, 0, sizeof(s_ethernet_info.ip_info));
    }
    taskEXIT_CRITICAL(&s_ethernet_lock);
}

static void ethernet_release_resources(void)
{
    if (s_eth_ip_event_instance) {
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, s_eth_ip_event_instance);
        s_eth_ip_event_instance = NULL;
    }
    if (s_eth_event_instance) {
        esp_event_handler_instance_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, s_eth_event_instance);
        s_eth_event_instance = NULL;
    }
    if (s_eth_handle) {
        esp_eth_stop(s_eth_handle);
    }
    if (s_eth_glue) {
        esp_eth_del_netif_glue(s_eth_glue);
        s_eth_glue = NULL;
    }
    if (s_eth_netif) {
        esp_netif_destroy(s_eth_netif);
        s_eth_netif = NULL;
    }
    if (s_eth_handle) {
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
    }
    if (s_eth_mac) {
        s_eth_mac->del(s_eth_mac);
        s_eth_mac = NULL;
    }
    if (s_eth_phy) {
        s_eth_phy->del(s_eth_phy);
        s_eth_phy = NULL;
    }
}

esp_err_t bsp_extra_ethernet_init(void)
{
    taskENTER_CRITICAL(&s_ethernet_lock);
    bool initialized = s_ethernet_info.initialized;
    taskEXIT_CRITICAL(&s_ethernet_lock);
    if (initialized) {
        return ESP_OK;
    }
    if (s_ethernet_initializing) {
        return ESP_ERR_INVALID_STATE;
    }
    s_ethernet_initializing = true;

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        goto fail;
    }
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        goto fail;
    }

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();

    emac_config.smi_gpio.mdc_num = BSP_EXTRA_ETH_MDC_GPIO;
    emac_config.smi_gpio.mdio_num = BSP_EXTRA_ETH_MDIO_GPIO;
    emac_config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
    emac_config.clock_config.rmii.clock_gpio = BSP_EXTRA_ETH_RMII_CLOCK_GPIO;
    emac_config.emac_dataif_gpio.rmii.tx_en_num = BSP_EXTRA_ETH_TX_EN_GPIO;
    emac_config.emac_dataif_gpio.rmii.txd0_num = BSP_EXTRA_ETH_TXD0_GPIO;
    emac_config.emac_dataif_gpio.rmii.txd1_num = BSP_EXTRA_ETH_TXD1_GPIO;
    emac_config.emac_dataif_gpio.rmii.crs_dv_num = BSP_EXTRA_ETH_CRS_DV_GPIO;
    emac_config.emac_dataif_gpio.rmii.rxd0_num = BSP_EXTRA_ETH_RXD0_GPIO;
    emac_config.emac_dataif_gpio.rmii.rxd1_num = BSP_EXTRA_ETH_RXD1_GPIO;

    phy_config.phy_addr = BSP_EXTRA_ETH_PHY_ADDRESS;
    phy_config.reset_gpio_num = BSP_EXTRA_ETH_PHY_RESET_GPIO;

    s_eth_mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
    ESP_GOTO_ON_FALSE(s_eth_mac, ESP_ERR_NO_MEM, fail, TAG, "Create Ethernet MAC failed");
    s_eth_phy = esp_eth_phy_new_ip101(&phy_config);
    ESP_GOTO_ON_FALSE(s_eth_phy, ESP_ERR_NO_MEM, fail, TAG, "Create IP101 PHY failed");

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(s_eth_mac, s_eth_phy);
    ESP_GOTO_ON_ERROR(esp_eth_driver_install(&eth_config, &s_eth_handle), fail, TAG, "Install Ethernet driver failed");

    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_config);
    ESP_GOTO_ON_FALSE(s_eth_netif, ESP_ERR_NO_MEM, fail, TAG, "Create Ethernet netif failed");
    s_eth_glue = esp_eth_new_netif_glue(s_eth_handle);
    ESP_GOTO_ON_FALSE(s_eth_glue, ESP_ERR_NO_MEM, fail, TAG, "Create Ethernet netif glue failed");
    ESP_GOTO_ON_ERROR(esp_netif_attach(s_eth_netif, s_eth_glue), fail, TAG, "Attach Ethernet netif failed");

    ESP_GOTO_ON_ERROR(
        esp_event_handler_instance_register(
            ETH_EVENT,
            ESP_EVENT_ANY_ID,
            ethernet_event_handler,
            NULL,
            &s_eth_event_instance
        ),
        fail,
        TAG,
        "Register Ethernet event handler failed"
    );
    ESP_GOTO_ON_ERROR(
        esp_event_handler_instance_register(
            IP_EVENT,
            ESP_EVENT_ANY_ID,
            ethernet_ip_event_handler,
            NULL,
            &s_eth_ip_event_instance
        ),
        fail,
        TAG,
        "Register Ethernet IP event handler failed"
    );

    ESP_GOTO_ON_ERROR(esp_eth_start(s_eth_handle), fail, TAG, "Start Ethernet failed");

    taskENTER_CRITICAL(&s_ethernet_lock);
    s_ethernet_info.initialized = true;
    s_ethernet_info.last_error = ESP_OK;
    taskEXIT_CRITICAL(&s_ethernet_lock);
    s_ethernet_initializing = false;
    ESP_LOGI(TAG, "IP101 Ethernet started");
    return ESP_OK;

fail:
    ethernet_set_error(ret);
    ethernet_release_resources();
    s_ethernet_initializing = false;
    return ret;
}

esp_err_t bsp_extra_ethernet_get_info(bsp_extra_ethernet_info_t *info)
{
    ESP_RETURN_ON_FALSE(info, ESP_ERR_INVALID_ARG, TAG, "Ethernet info is NULL");
    taskENTER_CRITICAL(&s_ethernet_lock);
    *info = s_ethernet_info;
    taskEXIT_CRITICAL(&s_ethernet_lock);
    return ESP_OK;
}

esp_err_t bsp_extra_rs485_init(void)
{
    if (s_rs485_initialized) {
        return ESP_OK;
    }

    uart_config_t config = {
        .baud_rate = BSP_EXTRA_RS485_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(
        uart_driver_install(
            BSP_EXTRA_RS485_UART,
            BSP_EXTRA_RS485_RX_BUFFER_SIZE,
            BSP_EXTRA_RS485_TX_BUFFER_SIZE,
            0,
            NULL,
            0
        ),
        TAG,
        "Install RS485 UART driver failed"
    );

    esp_err_t ret = uart_param_config(BSP_EXTRA_RS485_UART, &config);
    if (ret == ESP_OK) {
        ret = uart_set_pin(
                  BSP_EXTRA_RS485_UART,
                  BSP_EXTRA_RS485_TX_GPIO,
                  BSP_EXTRA_RS485_RX_GPIO,
                  UART_PIN_NO_CHANGE,
                  UART_PIN_NO_CHANGE
              );
    }
    if (ret == ESP_OK) {
        ret = uart_set_mode(BSP_EXTRA_RS485_UART, UART_MODE_UART);
    }
    if (ret != ESP_OK) {
        uart_driver_delete(BSP_EXTRA_RS485_UART);
        return ret;
    }

    uart_flush_input(BSP_EXTRA_RS485_UART);
    s_rs485_initialized = true;
    return ESP_OK;
}

esp_err_t bsp_extra_rs485_write(const void *data, size_t size, size_t *bytes_written)
{
    ESP_RETURN_ON_FALSE(data && size > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid RS485 TX buffer");
    ESP_RETURN_ON_ERROR(bsp_extra_rs485_init(), TAG, "RS485 initialization failed");

    int written = uart_write_bytes(BSP_EXTRA_RS485_UART, data, size);
    ESP_RETURN_ON_FALSE(written >= 0, ESP_FAIL, TAG, "RS485 write failed");
    if (bytes_written) {
        *bytes_written = (size_t)written;
    }
    return written == (int)size ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

esp_err_t bsp_extra_rs485_read(void *data, size_t size, size_t *bytes_read, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(data && size > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid RS485 RX buffer");
    ESP_RETURN_ON_ERROR(bsp_extra_rs485_init(), TAG, "RS485 initialization failed");

    int read = uart_read_bytes(BSP_EXTRA_RS485_UART, data, size, pdMS_TO_TICKS(timeout_ms));
    ESP_RETURN_ON_FALSE(read >= 0, ESP_FAIL, TAG, "RS485 read failed");
    if (bytes_read) {
        *bytes_read = (size_t)read;
    }
    return ESP_OK;
}
