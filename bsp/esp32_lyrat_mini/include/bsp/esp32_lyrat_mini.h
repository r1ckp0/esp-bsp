/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP BSP: ESP32-LyraT-Mini v1.2
 */

#pragma once

#include "sdkconfig.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_codec_dev.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"

#include "iot_button.h"
#include "led_indicator.h"

#include "bsp/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 *  BSP Board Name
 **************************************************************************************************/

/**
 * @defgroup boardname Board Name
 * @brief BSP Board Name
 * @{
 */

#define BSP_BOARD_ESP32_LYRAT_MINI

/** @} */

/**************************************************************************************************
 *  BSP Capabilities
 **************************************************************************************************/

/**
 * @defgroup capabilities Capabilities
 * @brief BSP Capabilities
 * @{
 */

#define BSP_CAPS_DISPLAY          0
#define BSP_CAPS_TOUCH            0
#define BSP_CAPS_BUTTONS          1
#define BSP_CAPS_KNOB             0
#define BSP_CAPS_AUDIO            1
#define BSP_CAPS_AUDIO_SPEAKER    1
#define BSP_CAPS_AUDIO_MIC        1
#define BSP_CAPS_SDCARD           1
#define BSP_CAPS_LED              1
#define BSP_CAPS_CAMERA           0
#define BSP_CAPS_BAT              0
#define BSP_CAPS_IMU              0

/** @} */

/**************************************************************************************************
 *  Board pinout
 **************************************************************************************************/

/**
 * @defgroup g01_i2c I2C
 * @brief I2C BSP API
 * @{
 */

/**
 * I2C compartido por ES8311 y ES7243.
 *
 * Codec_I2C_SDA -> GPIO18
 * Codec_I2C_SCL -> GPIO23
 */
#define BSP_I2C_SDA                 GPIO_NUM_18
#define BSP_I2C_SCL                 GPIO_NUM_23

/** @} */

/**
 * @defgroup g03_audio Audio
 * @brief Audio BSP API
 * @{
 */

/*
 * Audio master clock.
 *
 * GPIO0 está conectado al MCLK del ES7243 y al CCLK/MCLK
 * del ES8311 según el esquemático de LyraT-Mini v1.2.
 */
#define BSP_I2S_MCLK                 GPIO_NUM_0

#define BSP_I2S0_NUM                 I2S_NUM_0
#define BSP_I2S1_NUM                 I2S_NUM_1

/*
 * I2S0: ES8311
 *
 * ES8311 MCLK   <- GPIO0
 * ES8311 SCLK   <- GPIO5
 * ES8311 LRCK   <- GPIO25
 * ES8311 SDIN   <- GPIO26  (ESP32 -> ES8311)
 * ES8311 SDOUT  -> GPIO4   (ES8311 -> ESP32)
 */
#define BSP_I2S0_SCLK                GPIO_NUM_5
#define BSP_I2S0_LCLK                GPIO_NUM_25
#define BSP_I2S0_DOUT                GPIO_NUM_26
#define BSP_I2S0_DSIN                GPIO_NUM_4

/*
 * I2S1: ES7243
 *
 * ES7243 MCLK   <- GPIO0
 * ES7243 SCLK   <- GPIO32
 * ES7243 LRCK   <- GPIO33
 * ES7243 SDOUT  -> GPIO35
 *
 * ES7243 es un ADC de entrada: no necesita una línea DOUT
 * desde el ESP32 hacia el codec.
 */
#define BSP_I2S1_SCLK                GPIO_NUM_32
#define BSP_I2S1_LCLK                GPIO_NUM_33
#define BSP_I2S1_DSIN                GPIO_NUM_35
#define BSP_I2S1_DOUT                GPIO_NUM_NC

/*
 * Amplificador NS4150.
 *
 * PA_CTRL:
 *   GPIO21 = 1 -> amplificador habilitado
 *   GPIO21 = 0 -> amplificador deshabilitado
 */
#define BSP_POWER_AMP_IO             GPIO_NUM_21
#define BSP_POWER_AMP_ACTIVE_LEVEL   1

/** @} */

/**
 * @defgroup g05_buttons Buttons
 * @brief Buttons BSP API
 * @{
 */

/*
 * Los seis botones están conectados mediante una escalera resistiva.
 *
 * Button_Array_ADC -> GPIO39
 * GPIO39 corresponde a ADC1_CHANNEL_3 en ESP32.
 */
#define BSP_BUTTON_ADC_GPIO          GPIO_NUM_39
#define BSP_BUTTON_ADC_UNIT          ADC_UNIT_1
#define BSP_BUTTON_ADC_CHANNEL       ADC_CHANNEL_3
#define BSP_BUTTON_ADC_ATTEN         ADC_ATTEN_DB_12

/** @} */

/**
 * @defgroup g06_led LEDs
 * @brief LEDs BSP API
 * @{
 */

/*
 * LED verde:
 *   GPIO22 -> transistor Q6 -> LED verde.
 *
 * LED azul:
 *   GPIO27 -> transistor Q8 -> LED azul.
 *
 * Ambos transistores son NPN a masa, por tanto:
 * GPIO alto -> LED encendido.
 */
#define BSP_LED_GREEN_IO             GPIO_NUM_22
#define BSP_LED_BLUE_IO              GPIO_NUM_27

/** @} */

/**
 * @defgroup g02_storage SD Card and SPIFFS
 * @brief SPIFFS and SD card BSP API
 * @{
 */

/*
 * MicroSD: SDMMC 1-bit mode.
 *
 * SD_HOST_CLK  -> GPIO14
 * SD_HOST_CMD  -> GPIO15
 * SD_HOST_DATA -> GPIO2
 * SD_DET       -> GPIO34
 * SD_PWR_CTRL  -> GPIO13
 *
 * Las señales DAT1, DAT2 y DAT3 no están cableadas como líneas
 * activas de datos para la tarjeta en esta placa. Por ese motivo,
 * el BSP debe usar SDMMC en modo de un bit.
 */
#define BSP_SD_CLK                   GPIO_NUM_14
#define BSP_SD_CMD                   GPIO_NUM_15
#define BSP_SD_D0                    GPIO_NUM_2
#define BSP_SD_D1                    GPIO_NUM_NC
#define BSP_SD_D2                    GPIO_NUM_NC
#define BSP_SD_D3                    GPIO_NUM_NC
#define BSP_SD_DET                   GPIO_NUM_34
#define BSP_SD_POWER                 GPIO_NUM_13

/** @} */

/**************************************************************************************************
 *  Enumerations
 **************************************************************************************************/

/**
 * @addtogroup g05_buttons
 * @{
 */

/**
 * @brief Logical button identifiers
 *
 * Physical order in the schematic:
 *
 * - REC
 * - MODE
 * - PLAY
 * - SET
 * - VOL-
 * - VOL+
 */
typedef enum {
    BSP_BUTTON_REC = 0,
    BSP_BUTTON_MODE,
    BSP_BUTTON_PLAY,
    BSP_BUTTON_SET,
    BSP_BUTTON_VOLDOWN,
    BSP_BUTTON_VOLUP,
    BSP_BUTTON_NUM
} bsp_button_t;

/** @} */

/**
 * @addtogroup g06_led
 * @{
 */

typedef enum {
    BSP_LED_GREEN = 0,
    BSP_LED_BLUE,
    BSP_LED_NUM
} bsp_led_t;

/**
 * @brief Default LED effects
 */
typedef enum {
    BSP_LED_ON = 0,
    BSP_LED_OFF,
    BSP_LED_BLINK_FAST,
    BSP_LED_BLINK_SLOW,
    BSP_LED_MAX,
} bsp_led_effect_t;

/** @} */

/**************************************************************************************************
 *  Audio API
 **************************************************************************************************/

/**
 * @addtogroup g03_audio
 * @{
 */

/**
 * @brief Initialize audio I2S interfaces
 *
 * The board has two separate I2S interfaces:
 *
 * - I2S_NUM_0 for ES8311 speaker codec.
 * - I2S_NUM_1 for ES7243 microphone ADC.
 *
 * @param[in] i2s_config Optional ES8311 I2S standard-mode configuration.
 *                       Pass NULL to use BSP defaults.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if configuration is invalid
 *      - ESP_ERR_NOT_FOUND if no suitable I2S channel is available
 *      - ESP_ERR_NO_MEM on allocation failure
 *      - ESP_ERR_INVALID_STATE if I2S is already initialized
 */
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config);

/**
 * @brief Get speaker codec I2S interface
 *
 * This returns the I2S data interface associated with ES8311.
 *
 * @return Pointer to codec data interface, or NULL on error.
 */
const audio_codec_data_if_t *bsp_audio_get_codec_itf(void);

/**
 * @brief Get microphone ADC I2S interface
 *
 * This returns the I2S data interface associated with ES7243.
 *
 * @return Pointer to codec data interface, or NULL on error.
 */
const audio_codec_data_if_t *bsp_audio_get_microphone_codec_itf(void);

/**
 * @brief Initialize ES8311 speaker codec device
 *
 * This initializes:
 *
 * - I2C bus, if needed.
 * - I2S0 interface.
 * - ES8311 codec control interface.
 * - `esp_codec_dev` playback device.
 *
 * @return Speaker codec device handle, or NULL on error.
 */
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);

/**
 * @brief Initialize ES7243 microphone codec device
 *
 * This initializes:
 *
 * - I2C bus, if needed.
 * - I2S1 interface.
 * - ES7243 codec control interface.
 * - `esp_codec_dev` recording device.
 *
 * @return Microphone codec device handle, or NULL on error.
 */
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void);

/**
 * @brief Enable or disable NS4150 speaker amplifier
 *
 * @param[in] enable true enables the amplifier; false disables it.
 *
 * @return ESP_OK on success.
 */
esp_err_t bsp_audio_poweramp_enable(bool enable);

/** @} */

/**************************************************************************************************
 *  I2C API
 **************************************************************************************************/

/**
 * @addtogroup g01_i2c
 * @{
 */

#define BSP_I2C_NUM                 CONFIG_BSP_I2C_NUM

/**
 * @brief Initialize the I2C master bus used by ES8311 and ES7243
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG for invalid parameters
 *      - ESP_ERR_NO_MEM if allocation fails
 *      - ESP_ERR_INVALID_STATE if the bus is already initialized
 */
esp_err_t bsp_i2c_init(void);

/**
 * @brief Deinitialize the BSP I2C master bus
 *
 * @return ESP_OK on success.
 */
esp_err_t bsp_i2c_deinit(void);

/**
 * @brief Get I2C master bus handle
 *
 * @return I2C master bus handle, or NULL if I2C is not initialized.
 */
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

/** @} */

/**************************************************************************************************
 *  SPIFFS API
 **************************************************************************************************/

/**
 * @addtogroup g02_storage
 * @{
 */

#define BSP_SPIFFS_MOUNT_POINT      CONFIG_BSP_SPIFFS_MOUNT_POINT

/**
 * @brief Mount SPIFFS filesystem
 *
 * @return ESP_OK on success, or an error from esp_vfs_spiffs_register().
 */
esp_err_t bsp_spiffs_mount(void);

/**
 * @brief Unmount SPIFFS filesystem
 *
 * @return ESP_OK on success.
 */
esp_err_t bsp_spiffs_unmount(void);

/**************************************************************************************************
 *  SD card API
 **************************************************************************************************/

#define BSP_SD_MOUNT_POINT          CONFIG_BSP_SD_MOUNT_POINT

typedef struct {
    const esp_vfs_fat_sdmmc_mount_config_t *mount;
    sdmmc_host_t *host;
    union {
        const sdmmc_slot_config_t   *sdmmc;
        const sdspi_device_config_t *sdspi;
    } slot;
} bsp_sdcard_cfg_t;

/**
 * @brief Mount microSD card using the default SDMMC 1-bit configuration
 *
 * @return ESP_OK on success.
 */
esp_err_t bsp_sdcard_mount(void);

/**
 * @brief Unmount microSD card
 *
 * @return ESP_OK on success.
 */
esp_err_t bsp_sdcard_unmount(void);

/**
 * @brief Get mounted SD card handle
 *
 * @return SD card handle, or NULL if card is not mounted.
 */
sdmmc_card_t *bsp_sdcard_get_handle(void);

/**
 * @brief Get SDMMC host configuration
 *
 * @param[in] slot SD slot number.
 * @param[out] config Output SDMMC host configuration.
 */
void bsp_sdcard_get_sdmmc_host(const int slot, sdmmc_host_t *config);

/**
 * @brief Get SDSPI host configuration
 *
 * @param[in] slot SPI slot number.
 * @param[out] config Output SDSPI host configuration.
 */
void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config);

/**
 * @brief Get SDMMC slot configuration
 *
 * The LyraT Mini supports SDMMC in 1-bit mode.
 *
 * @param[in] slot SDMMC slot number.
 * @param[out] config Output slot configuration.
 */
void bsp_sdcard_sdmmc_get_slot(const int slot, sdmmc_slot_config_t *config);

/**
 * @brief Get SDSPI slot configuration
 *
 * @param[in] spi_host SPI host ID.
 * @param[out] config Output SPI slot configuration.
 */
void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host,
                               sdspi_device_config_t *config);

/**
 * @brief Mount SD card using an explicit SDMMC configuration
 *
 * @param[in] cfg SD card configuration.
 *
 * @return ESP_OK on success.
 */
esp_err_t bsp_sdcard_sdmmc_mount(bsp_sdcard_cfg_t *cfg);

/**
 * @brief Mount SD card using an explicit SDSPI configuration
 *
 * This board's default wiring is SDMMC 1-bit. This API is retained
 * for compatibility with common ESP-BSP storage API.
 *
 * @param[in] cfg SD card configuration.
 *
 * @return ESP_OK on success.
 */
esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg);

/** @} */

/**************************************************************************************************
 *  ADC API
 **************************************************************************************************/

/**
 * @defgroup g01_adc ADC
 * @brief ADC BSP API
 * @{
 */

#define BSP_ADC_UNIT                ADC_UNIT_1

/**
 * @brief Initialize ADC one-shot driver used by button resistor ladder
 *
 * @return ESP_OK on success.
 */
esp_err_t bsp_adc_initialize(void);

/**
 * @brief Get ADC one-shot unit handle
 *
 * @return ADC handle, or NULL if ADC was not initialized.
 */
adc_oneshot_unit_handle_t bsp_adc_get_handle(void);

/** @} */

/**************************************************************************************************
 *  LEDs API
 **************************************************************************************************/

/**
 * @addtogroup g06_led
 * @{
 */

/**
 * @brief Create LED indicator handles for green and blue LEDs
 *
 * @param[out] led_array Output array for LED handles.
 * @param[out] led_cnt Number of returned LED handles; may be NULL.
 * @param[in] led_array_size Must be at least BSP_LED_NUM.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if arguments are invalid
 *      - ESP_FAIL if LED indicator creation fails
 */
esp_err_t bsp_led_indicator_create(led_indicator_handle_t led_array[],
                                   int *led_cnt,
                                   int led_array_size);

/**
 * @brief Turn an LED on or off
 *
 * @param[in] handle LED indicator handle.
 * @param[in] on true turns LED on; false turns LED off.
 *
 * @return ESP_OK on success.
 */
esp_err_t bsp_led_set(led_indicator_handle_t handle, const bool on);

/** @} */

/**************************************************************************************************
 *  Buttons API
 **************************************************************************************************/

/**
 * @addtogroup g05_buttons
 * @{
 */

/**
 * @brief Create handlers for six ADC resistor-ladder buttons
 *
 * The buttons are connected to GPIO39 / ADC1_CHANNEL_3:
 *
 * - REC
 * - MODE
 * - PLAY
 * - SET
 * - VOL-
 * - VOL+
 *
 * @note ADC thresholds must be adjusted against actual board measurements.
 *
 * @param[out] btn_array Output array of button handles.
 * @param[out] btn_cnt Number of created button handles; may be NULL.
 * @param[in] btn_array_size Must be at least BSP_BUTTON_NUM.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if btn_array is NULL or too small
 *      - ESP_FAIL if an underlying button driver creation fails
 */
esp_err_t bsp_iot_button_create(button_handle_t btn_array[],
                                int *btn_cnt,
                                int btn_array_size);

/** @} */

#ifdef __cplusplus
}
#endif