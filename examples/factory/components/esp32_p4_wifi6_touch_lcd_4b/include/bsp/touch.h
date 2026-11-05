#pragma once
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BSP touch configuration structure
 *
 */
typedef struct {
    struct {
        unsigned int swap_xy: 1;   /*!< Swap X and Y after reading coordinates */
        unsigned int mirror_x: 1;  /*!< Mirror X after reading coordinates */
        unsigned int mirror_y: 1;  /*!< Mirror Y after reading coordinates */
    } flags;
} bsp_touch_config_t;

/**
 * @brief Create new touchscreen
 *
 * The returned handle is owned by Board Manager for the factory lifetime and
 * must not be deleted by callers.
 *
 * @param[in]  config    touch configuration
 * @param[out] ret_touch esp_lcd_touch touchscreen handle
 * @return
 *      - ESP_OK         On success
 *      - Else           esp_lcd_touch failure
 */
esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch);

#ifdef __cplusplus
}
#endif
