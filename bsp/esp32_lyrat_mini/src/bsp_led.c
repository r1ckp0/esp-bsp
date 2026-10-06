/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp32_lyrat_mini_led.c
 * @brief LED support for ESP32-LyraT-Mini v1.2
 *
 * Board LEDs:
 *
 * - Green LED: GPIO22
 * - Blue LED:  GPIO27
 *
 * Both LEDs are controlled through NPN transistors connected to ground.
 * Therefore, a high GPIO level turns the corresponding LED on.
 */

#include "esp_err.h"

#include "bsp_err_check.h"
#include "led_indicator_gpio.h"

#include "bsp/esp32_lyrat_mini.h"

/*
 * Default blink patterns are supplied by the esp-bsp common LED support.
 *
 * The array normally contains patterns corresponding to:
 *
 * BSP_LED_ON
 * BSP_LED_OFF
 * BSP_LED_BLINK_FAST
 * BSP_LED_BLINK_SLOW
 */
extern blink_step_t const *bsp_led_blink_defaults_lists[];

static led_indicator_gpio_config_t s_bsp_leds_gpio_config[BSP_LED_NUM] = {
    [BSP_LED_GREEN] = {
        .is_active_level_high = 1,
        .gpio_num = BSP_LED_GREEN_IO,
    },
    [BSP_LED_BLUE] = {
        .is_active_level_high = 1,
        .gpio_num = BSP_LED_BLUE_IO,
    },
};

static const led_indicator_config_t s_bsp_leds_config = {
    .blink_lists = bsp_led_blink_defaults_lists,
    .blink_list_num = BSP_LED_MAX,
};

esp_err_t bsp_led_indicator_create(led_indicator_handle_t led_array[],
                                   int *led_cnt,
                                   int led_array_size)
{
    if (led_array == NULL || led_array_size < BSP_LED_NUM) {
        return ESP_ERR_INVALID_ARG;
    }

    if (led_cnt != NULL) {
        *led_cnt = 0;
    }

    for (int i = 0; i < BSP_LED_NUM; i++) {
        esp_err_t ret = led_indicator_new_gpio_device(
                            &s_bsp_leds_config,
                            &s_bsp_leds_gpio_config[i],
                            &led_array[i]
                        );

        BSP_ERROR_CHECK_RETURN_ERR(ret);

        if (led_cnt != NULL) {
            (*led_cnt)++;
        }
    }

    return ESP_OK;
}

esp_err_t bsp_led_set(led_indicator_handle_t handle, const bool on)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return led_indicator_start(
               handle,
               on ? BSP_LED_ON : BSP_LED_OFF
           );
}