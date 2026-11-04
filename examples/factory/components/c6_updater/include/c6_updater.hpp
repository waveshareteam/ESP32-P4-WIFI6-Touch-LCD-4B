#pragma once

#include "driver/uart.h"

/* Receives one validated C6 app image over uart and installs it through ESP-Hosted. */
void c6_updater_receive_and_install(uart_port_t uart, const char *arguments);

/* Installs the factory-bundled C6 image. This is an explicit operator action. */
void c6_updater_install_bundled(uart_port_t uart);
