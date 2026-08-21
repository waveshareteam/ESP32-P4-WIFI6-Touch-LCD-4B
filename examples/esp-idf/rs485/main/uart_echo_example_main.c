#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_ldo_regulator.h"
#include "sdkconfig.h"
#include "esp_log.h"

#define ECHO_TEST_TXD CONFIG_EXAMPLE_UART_TXD
#define ECHO_TEST_RXD CONFIG_EXAMPLE_UART_RXD
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      CONFIG_EXAMPLE_UART_PORT_NUM
#define ECHO_UART_BAUD_RATE     CONFIG_EXAMPLE_UART_BAUD_RATE
#define ECHO_TASK_STACK_SIZE    CONFIG_EXAMPLE_TASK_STACK_SIZE

static const char *TAG = "UART TEST";

#define BUF_SIZE (1024)

static esp_err_t bsp_enable_ldo_vo4(void)
{
    static esp_ldo_channel_handle_t vo4_chan = NULL;
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = 4,
        .voltage_mv = 3300,
    };

    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &vo4_chan));
    ESP_LOGI(TAG, "LDO VO4 set to 3300mV");

    return ESP_OK;
}

static void echo_task(void *arg)
{
    (void)arg;
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate UART buffer");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        // Read data from the UART
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            // The current 86-panel bottom board controls RS485 direction automatically.
            uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) data, len);
            data[len] = '\0';
            ESP_LOGI(TAG, "Recv str: %s", (char *) data);
        } else if (len < 0) {
            ESP_LOGE(TAG, "UART read failed");
        }
    }
}

void app_main(void)
{

    ESP_ERROR_CHECK(bsp_enable_ldo_vo4());
    BaseType_t created = xTaskCreate(echo_task, "uart_echo_task", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_FAIL);
}
