/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp32_lyrat_mini_i2c.c
 * @brief I2C master bus support for ESP32-LyraT-Mini v1.2
 *
 * Board connections:
 *
 *   GPIO18 -> Codec_I2C_SDA
 *   GPIO23 -> Codec_I2C_SCL
 *
 * Devices on this bus:
 *
 *   - ES8311: audio DAC/ADC codec
 *   - ES7243: microphone ADC
 */

#include "esp_err.h"

#include "bsp_err_check.h"
#include "bsp/esp32_lyrat_mini.h"

static i2c_master_bus_handle_t s_i2c_handle = NULL;
static bool s_i2c_initialized = false;

esp_err_t bsp_i2c_init(void)
{
    if (s_i2c_initialized) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t i2c_config = {
        .i2c_port = BSP_I2C_NUM,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    BSP_ERROR_CHECK_RETURN_ERR(
        i2c_new_master_bus(&i2c_config, &s_i2c_handle)
    );

    s_i2c_initialized = true;

    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    /*
     * Llamar deinit antes de init no debe terminar intentando borrar
     * un handle NULL.
     */
    if (!s_i2c_initialized || s_i2c_handle == NULL) {
        return ESP_OK;
    }

    BSP_ERROR_CHECK_RETURN_ERR(
        i2c_del_master_bus(s_i2c_handle)
    );

    s_i2c_handle = NULL;
    s_i2c_initialized = false;

    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    /*
     * Mantenemos el comportamiento del BSP Korvo-2:
     * solicitar el handle inicializa el bus si aún no existe.
     *
     * Esta función no puede propagar un esp_err_t; en caso de fallo,
     * devuelve NULL.
     */
    if (bsp_i2c_init() != ESP_OK) {
        return NULL;
    }

    return s_i2c_handle;
}