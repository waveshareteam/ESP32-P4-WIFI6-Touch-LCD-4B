/*
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <stddef.h>
#include "esp_err.h"

namespace esp_brookesia {
namespace systems {
namespace base {
class App;
}
namespace phone {
class Phone;
}
}
}

struct factory_diagnostics_app_t {
    const char *command_name;
    esp_brookesia::systems::base::App *app;
};

/**
 * Start the factory UART diagnostics service.  The service only observes the
 * existing display, I2C bus and touch input; it does not reconfigure them.
 */
esp_err_t factory_diagnostics_start(
    esp_brookesia::systems::phone::Phone *phone,
    const factory_diagnostics_app_t *apps,
    size_t app_count
);
