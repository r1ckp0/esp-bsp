/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp32_lyrat_mini_button.c
 * @brief Button support for ESP32-LyraT-Mini v1.2
 *
 * The board has six buttons connected through a resistor ladder:
 *
 * GPIO39 -> ADC1_CHANNEL_3
 *
 * Button order:
 *
 * - REC
 * - MODE
 * - PLAY
 * - SET
 * - VOL-
 * - VOL+
 */

#include "esp_err.h"
#include "esp_log.h"

#include "bsp_err_check.h"

#include "iot_button.h"
#include "button_gpio.h"
#include "button_adc.h"

#include "bsp/esp32_lyrat_mini.h"

static const char *TAG = "ESP32-LyraT-Mini";

/*
 * The button component receives a pointer to an ADC handle.
 *
 * This is intentionally a local static variable instead of directly
 * using the handle from esp32_lyrat_mini_adc.c. The ADC module owns
 * the actual driver handle; this variable stores its returned value
 * and gives button_adc a stable pointer.
 */
static adc_oneshot_unit_handle_t s_button_adc_handle = NULL;

typedef enum {
    BSP_BUTTON_TYPE_GPIO,
    BSP_BUTTON_TYPE_ADC,
} bsp_button_type_t;

typedef struct {
    bsp_button_type_t type;
    union {
        button_gpio_config_t gpio;
        button_adc_config_t adc;
    } cfg;
} bsp_button_config_t;

/*
 * Initial ADC thresholds for ESP32-LyraT-Mini v1.2.
 *
 * The values were calculated from nominal voltages indicated on the
 * board schematic:
 *
 *   REC:   2.41 V -> ~2990
 *   MODE:  1.98 V -> ~2458
 *   PLAY:  1.57 V -> ~1949
 *   SET:   1.18 V -> ~1464
 *   VOL-:  0.82 V -> ~1018
 *   VOL+:  0.38 V -> ~471
 *
 * Actual values must be verified on physical boards. Resistors are 5%
 * tolerance and ADC raw readings vary between ESP32 chips.
 */
static const bsp_button_config_t s_bsp_button_config[BSP_BUTTON_NUM] = {
    [BSP_BUTTON_REC] = {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_handle = &s_button_adc_handle,
            .adc_channel = BSP_BUTTON_ADC_CHANNEL,
            .button_index = BSP_BUTTON_REC,
            .min = 2310,
            .max = 2510,
        },
    },

    [BSP_BUTTON_MODE] = {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_handle = &s_button_adc_handle,
            .adc_channel = BSP_BUTTON_ADC_CHANNEL,
            .button_index = BSP_BUTTON_MODE,
            .min = 1880,
            .max = 2080,
        },
    },

    [BSP_BUTTON_PLAY] = {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_handle = &s_button_adc_handle,
            .adc_channel = BSP_BUTTON_ADC_CHANNEL,
            .button_index = BSP_BUTTON_PLAY,
            .min = 1470,
            .max = 1670,
        },
    },

    [BSP_BUTTON_SET] = {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_handle = &s_button_adc_handle,
            .adc_channel = BSP_BUTTON_ADC_CHANNEL,
            .button_index = BSP_BUTTON_SET,
            .min = 1080,
            .max = 1280,
        },
    },

    [BSP_BUTTON_VOLDOWN] = {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_handle = &s_button_adc_handle,
            .adc_channel = BSP_BUTTON_ADC_CHANNEL,
            .button_index = BSP_BUTTON_VOLDOWN,
            .min = 720,
            .max = 920,
        },
    },

    [BSP_BUTTON_VOLUP] = {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_handle = &s_button_adc_handle,
            .adc_channel = BSP_BUTTON_ADC_CHANNEL,
            .button_index = BSP_BUTTON_VOLUP,
            .min = 230,
            .max = 480,
        },
    },
};

esp_err_t bsp_iot_button_create(button_handle_t btn_array[],
                                int *btn_cnt,
                                int btn_array_size)
{
    if (btn_array == NULL || btn_array_size < BSP_BUTTON_NUM) {
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * adc_oneshot unit creation is idempotent in our BSP:
     * bsp_adc_initialize() returns ESP_OK when the unit already exists.
     */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_adc_initialize());

    s_button_adc_handle = bsp_adc_get_handle();

    if (s_button_adc_handle == NULL) {
        ESP_LOGE(TAG, "ADC unit handle is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    const button_config_t btn_config = {
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
    };

    esp_err_t ret = ESP_OK;

    if (btn_cnt != NULL) {
        *btn_cnt = 0;
    }

    for (int i = 0; i < BSP_BUTTON_NUM; i++) {
        button_handle_t button_handle = NULL;
        esp_err_t button_ret = ESP_OK;

        switch (s_bsp_button_config[i].type) {
        case BSP_BUTTON_TYPE_GPIO:
            button_ret = iot_button_new_gpio_device(
                             &btn_config,
                             &s_bsp_button_config[i].cfg.gpio,
                             &button_handle
                         );
            break;

        case BSP_BUTTON_TYPE_ADC:
            button_ret = iot_button_new_adc_device(
                             &btn_config,
                             &s_bsp_button_config[i].cfg.adc,
                             &button_handle
                         );
            break;

        default:
            ESP_LOGW(TAG, "Unsupported button type for button %d", i);
            button_ret = ESP_ERR_NOT_SUPPORTED;
            break;
        }

        if (button_ret != ESP_OK) {
            ESP_LOGE(TAG,
                     "Failed to create button %d, error: %s",
                     i,
                     esp_err_to_name(button_ret));

            /*
             * Preserve the first error. This is preferable to bitwise OR
             * because ESP error codes are not bit flags.
             */
            if (ret == ESP_OK) {
                ret = button_ret;
            }

            continue;
        }

        btn_array[i] = button_handle;

        if (btn_cnt != NULL) {
            (*btn_cnt)++;
        }
    }

    return ret;
}