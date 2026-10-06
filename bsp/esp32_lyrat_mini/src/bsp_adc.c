/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp32_lyrat_mini_adc.c
 * @brief ADC support for ESP32-LyraT-Mini v1.2
 *
 * The ADC is currently used by the resistor-ladder button array:
 *
 * GPIO39 -> ADC1_CHANNEL_3
 *
 * Buttons:
 * - REC
 * - MODE
 * - PLAY
 * - SET
 * - VOL-
 * - VOL+
 */

#include "bsp_err_check.h"
#include "bsp/esp32_lyrat_mini.h"

static adc_oneshot_unit_handle_t s_bsp_adc_handle = NULL;

esp_err_t bsp_adc_initialize(void)
{
    /* ADC was already initialized */
    if (s_bsp_adc_handle != NULL) {
        return ESP_OK;
    }

    const adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = BSP_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    BSP_ERROR_CHECK_RETURN_ERR(
        adc_oneshot_new_unit(&adc_init_config, &s_bsp_adc_handle)
    );

    return ESP_OK;
}

adc_oneshot_unit_handle_t bsp_adc_get_handle(void)
{
    return s_bsp_adc_handle;
}