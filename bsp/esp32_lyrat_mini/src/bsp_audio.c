/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp32_lyrat_mini_audio.c
 * @brief Audio BSP implementation for ESP32-LyraT-Mini v1.2
 *
 * Audio topology:
 *
 *   I2S0 -> ES8311:
 *      MCLK: GPIO0
 *      BCLK: GPIO5
 *      WS:   GPIO25
 *      DOUT: GPIO26  (ESP32 -> ES8311)
 *      DIN:  GPIO4   (ES8311 -> ESP32)
 *
 *   I2S1 -> ES7243:
 *      MCLK: GPIO_NUM_NC
 *            ES7243 receives shared MCLK from I2S0 GPIO0 driven by s_i2s0_tx_chan.
 *      BCLK: GPIO32
 *      WS:   GPIO33
 *      DOUT: unused
 *      DIN:  GPIO35  (ES7243 -> ESP32)
 */

#include <assert.h>

#include "esp_check.h"
#include "esp_err.h"

#include "bsp_err_check.h"
#include "bsp/esp32_lyrat_mini.h"

#include "esp_codec_dev_defaults.h"

static const char *TAG = "ESP32-LyraT-Mini";

/* I2S0: ES8311 */
static i2s_chan_handle_t s_i2s0_tx_chan = NULL;
static i2s_chan_handle_t s_i2s0_rx_chan = NULL;

/* I2S1: ES7243 */
static i2s_chan_handle_t s_i2s1_rx_chan = NULL;

/* Interfaces de datos para esp_codec_dev */
static const audio_codec_data_if_t *s_es8311_i2s_data_if = NULL;
static const audio_codec_data_if_t *s_es7243_i2s_data_if = NULL;

/* Handles de codec, para no crear el mismo dispositivo repetidamente */
static esp_codec_dev_handle_t s_speaker_codec = NULL;
static esp_codec_dev_handle_t s_microphone_codec = NULL;

/**
 * @brief GPIO configuration for ES8311 / I2S0
 */
#define BSP_I2S0_GPIO_CFG                       \
    {                                           \
        .mclk = BSP_I2S_MCLK,                   \
        .bclk = BSP_I2S0_SCLK,                  \
        .ws = BSP_I2S0_LCLK,                    \
        .dout = BSP_I2S0_DOUT,                  \
        .din = BSP_I2S0_DSIN,                   \
        .invert_flags = {                       \
            .mclk_inv = false,                  \
            .bclk_inv = false,                  \
            .ws_inv = false,                    \
        },                                      \
    }

/**
 * @brief GPIO configuration for ES7243 / I2S1
 *
 * MCLK debe ser GPIO_NUM_NC porque GPIO0 ya está siendo dirigido por
 * I2S0. ES7243 recibe el reloj compartido físicamente desde GPIO0.
 */
#define BSP_I2S1_GPIO_CFG                       \
    {                                           \
        .mclk = GPIO_NUM_NC,                    \
        .bclk = BSP_I2S1_SCLK,                  \
        .ws = BSP_I2S1_LCLK,                    \
        .dout = GPIO_NUM_NC,                    \
        .din = BSP_I2S1_DSIN,                   \
        .invert_flags = {                       \
            .mclk_inv = false,                  \
            .bclk_inv = false,                  \
            .ws_inv = false,                    \
        },                                      \
    }

/**
 * @brief Default ES8311 I2S configuration
 *
 * Mono, 16 bits, Philips standard with Stereo slot mode for hardware BCLK ratio.
 */
#define BSP_I2S0_DUPLEX_CFG(_sample_rate)                                           \
    {                                                                               \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                       \
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(                           \
                        I2S_DATA_BIT_WIDTH_16BIT,                                   \
                        I2S_SLOT_MODE_STEREO),                                      \
        .gpio_cfg = BSP_I2S0_GPIO_CFG,                                             \
    }

/**
 * @brief Default ES7243 I2S RX configuration
 */
#define BSP_I2S1_RX_CFG(_sample_rate)                                               \
    {                                                                               \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                       \
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(                           \
                        I2S_DATA_BIT_WIDTH_16BIT,                                   \
                        I2S_SLOT_MODE_STEREO),                                      \
        .gpio_cfg = BSP_I2S1_GPIO_CFG,                                             \
    }

/**
 * @brief Enable or disable NS4150 speaker amplifier
 */
esp_err_t bsp_audio_poweramp_enable(bool enable)
{
    const gpio_config_t io_config = {
        .pin_bit_mask = 1ULL << BSP_POWER_AMP_IO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(
        gpio_config(&io_config),
        TAG,
        "Failed to configure PA control GPIO"
    );

    return gpio_set_level(
               BSP_POWER_AMP_IO,
               enable ? BSP_POWER_AMP_ACTIVE_LEVEL :
                        !BSP_POWER_AMP_ACTIVE_LEVEL
           );
}

/**
 * @brief Initialize I2S0 for ES8311 and I2S1 for ES7243
 */
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    esp_err_t ret = ESP_FAIL;

    if (s_es8311_i2s_data_if != NULL &&
        s_es7243_i2s_data_if != NULL) {
        return ESP_OK;
    }

    const i2s_std_config_t i2s0_default_cfg =
        BSP_I2S0_DUPLEX_CFG(16000);

    const i2s_std_config_t *i2s0_cfg = &i2s0_default_cfg;

    if (i2s_config != NULL) {
        i2s0_cfg = i2s_config;
    }

    const uint32_t sample_rate =
        i2s0_cfg->clk_cfg.sample_rate_hz;

    const i2s_std_config_t i2s1_default_cfg =
        BSP_I2S1_RX_CFG(sample_rate);

    BSP_ERROR_CHECK_RETURN_ERR(
        bsp_audio_poweramp_enable(false)
    );

    /* Crear I2S0 */
    const i2s_chan_config_t i2s0_chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(BSP_I2S0_NUM, I2S_ROLE_MASTER);

    ESP_GOTO_ON_ERROR(
        i2s_new_channel(
            &i2s0_chan_cfg,
            &s_i2s0_tx_chan,
            &s_i2s0_rx_chan
        ),
        err,
        TAG,
        "Failed to create I2S0 channels"
    );

    ESP_GOTO_ON_ERROR(
        i2s_channel_init_std_mode(
            s_i2s0_tx_chan,
            i2s0_cfg
        ),
        err,
        TAG,
        "Failed to initialize I2S0 TX channel"
    );

    ESP_GOTO_ON_ERROR(
        i2s_channel_init_std_mode(
            s_i2s0_rx_chan,
            i2s0_cfg
        ),
        err,
        TAG,
        "Failed to initialize I2S0 RX channel"
    );

    /* Habilitar s_i2s0_tx_chan para generar el reloj MCLK continuo en GPIO0 */
    ESP_GOTO_ON_ERROR(
        i2s_channel_enable(s_i2s0_tx_chan),
        err,
        TAG,
        "Failed to enable I2S0 TX channel for MCLK"
    );

    /* Crear I2S1 */
    const i2s_chan_config_t i2s1_chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(BSP_I2S1_NUM, I2S_ROLE_MASTER);

    ESP_GOTO_ON_ERROR(
        i2s_new_channel(
            &i2s1_chan_cfg,
            NULL,
            &s_i2s1_rx_chan
        ),
        err,
        TAG,
        "Failed to create I2S1 RX channel"
    );

    ESP_GOTO_ON_ERROR(
        i2s_channel_init_std_mode(
            s_i2s1_rx_chan,
            &i2s1_default_cfg
        ),
        err,
        TAG,
        "Failed to initialize I2S1 RX channel"
    );

    /* Crear interfaz esp_codec_dev para ES8311 */
    audio_codec_i2s_cfg_t es8311_i2s_cfg = {
        .port = BSP_I2S0_NUM,
        .rx_handle = s_i2s0_rx_chan,
        .tx_handle = s_i2s0_tx_chan,
    };

    s_es8311_i2s_data_if =
        audio_codec_new_i2s_data(&es8311_i2s_cfg);

    BSP_NULL_CHECK_GOTO(
        s_es8311_i2s_data_if,
        err
    );

    /*
     * Crear interfaz esp_codec_dev para ES7243.
     * Asignamos s_i2s0_tx_chan como tx_handle para que cuando se active
     * la grabación se habilite también el canal I2S0 que suministra MCLK en GPIO0.
     */
    audio_codec_i2s_cfg_t es7243_i2s_cfg = {
        .port = BSP_I2S1_NUM,
        .rx_handle = s_i2s1_rx_chan,
        .tx_handle = s_i2s0_tx_chan,
    };

    s_es7243_i2s_data_if =
        audio_codec_new_i2s_data(&es7243_i2s_cfg);

    BSP_NULL_CHECK_GOTO(
        s_es7243_i2s_data_if,
        err
    );

    ESP_LOGI(
        TAG,
        "Audio I2S initialized: ES8311=I2S%d, ES7243=I2S%d, rate=%" PRIu32,
        BSP_I2S0_NUM,
        BSP_I2S1_NUM,
        sample_rate
    );

    return ESP_OK;

err:
    if (s_i2s1_rx_chan != NULL) {
        i2s_channel_disable(s_i2s1_rx_chan);
        i2s_del_channel(s_i2s1_rx_chan);
        s_i2s1_rx_chan = NULL;
    }

    if (s_i2s0_tx_chan != NULL) {
        i2s_channel_disable(s_i2s0_tx_chan);
        i2s_del_channel(s_i2s0_tx_chan);
        s_i2s0_tx_chan = NULL;
    }

    if (s_i2s0_rx_chan != NULL) {
        i2s_channel_disable(s_i2s0_rx_chan);
        i2s_del_channel(s_i2s0_rx_chan);
        s_i2s0_rx_chan = NULL;
    }

    s_es8311_i2s_data_if = NULL;
    s_es7243_i2s_data_if = NULL;

    return ret;
}

const audio_codec_data_if_t *bsp_audio_get_codec_itf(void)
{
    return s_es8311_i2s_data_if;
}

const audio_codec_data_if_t *bsp_audio_get_microphone_codec_itf(void)
{
    return s_es7243_i2s_data_if;
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    if (s_speaker_codec != NULL) {
        return s_speaker_codec;
    }

    if (s_es8311_i2s_data_if == NULL) {
        BSP_ERROR_CHECK_RETURN_NULL(bsp_i2c_init());
        BSP_ERROR_CHECK_RETURN_NULL(bsp_audio_init(NULL));
    }

    assert(s_es8311_i2s_data_if != NULL);

    const audio_codec_gpio_if_t *gpio_if =
        audio_codec_new_gpio();

    BSP_NULL_CHECK(gpio_if, NULL);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_I2C_NUM,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = bsp_i2c_get_handle(),
    };

    const audio_codec_ctrl_if_t *i2c_ctrl_if =
        audio_codec_new_i2c_ctrl(&i2c_cfg);

    BSP_NULL_CHECK(i2c_ctrl_if, NULL);

    const esp_codec_dev_hw_gain_t hw_gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };

    es8311_codec_cfg_t codec_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = BSP_POWER_AMP_IO,
        .pa_reverted = false,
        .master_mode = false,
        .hw_gain = hw_gain,
    };

    const audio_codec_if_t *codec_if =
        es8311_codec_new(&codec_cfg);

    BSP_NULL_CHECK(codec_if, NULL);

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if,
        .data_if = s_es8311_i2s_data_if,
    };

    s_speaker_codec = esp_codec_dev_new(&codec_dev_cfg);

    BSP_NULL_CHECK(s_speaker_codec, NULL);

    return s_speaker_codec;
}

esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    if (s_microphone_codec != NULL) {
        return s_microphone_codec;
    }

    if (s_es7243_i2s_data_if == NULL) {
        BSP_ERROR_CHECK_RETURN_NULL(bsp_i2c_init());
        BSP_ERROR_CHECK_RETURN_NULL(bsp_audio_init(NULL));
    }

    assert(s_es7243_i2s_data_if != NULL);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_I2C_NUM,
        .addr = ES7243_CODEC_DEFAULT_ADDR,
        .bus_handle = bsp_i2c_get_handle(),
    };

    const audio_codec_ctrl_if_t *i2c_ctrl_if =
        audio_codec_new_i2c_ctrl(&i2c_cfg);

    BSP_NULL_CHECK(i2c_ctrl_if, NULL);

    es7243_codec_cfg_t codec_cfg = {
        .ctrl_if = i2c_ctrl_if,
    };

    const audio_codec_if_t *codec_if =
        es7243_codec_new(&codec_cfg);

    BSP_NULL_CHECK(codec_if, NULL);

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = codec_if,
        .data_if = s_es7243_i2s_data_if,
    };

    s_microphone_codec = esp_codec_dev_new(&codec_dev_cfg);

    BSP_NULL_CHECK(s_microphone_codec, NULL);

    return s_microphone_codec;
}