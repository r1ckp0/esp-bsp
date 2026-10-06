/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp32_lyrat_mini_storage.c
 * @brief SPIFFS and microSD support for ESP32-LyraT-Mini v1.2
 *
 * microSD wiring:
 *
 *   SD_HOST_CLK   -> GPIO14
 *   SD_HOST_CMD   -> GPIO15
 *   SD_HOST_DATA0 -> GPIO2
 *   SD_DET        -> GPIO34
 *   SD_PWR_CTRL   -> GPIO13
 *
 * The board supports SDMMC in 1-bit mode.
 */

#include <assert.h>
#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"

#include "driver/gpio.h"
#include "driver/sdmmc_host.h"

#include "bsp_err_check.h"
#include "bsp/esp32_lyrat_mini.h"

static const char *TAG = "ESP32-LyraT-Mini";

/* Global mounted microSD card handle */
static sdmmc_card_t *s_bsp_sdcard = NULL;

/**
 * @brief Enable or disable microSD power supply
 *
 * On LyraT-Mini, GPIO13 controls Q9 and is active low:
 *
 * GPIO13 = 0 -> SD card power enabled
 * GPIO13 = 1 -> SD card power disabled
 */
static esp_err_t bsp_sdcard_power_enable(bool enable)
{
    const gpio_config_t io_config = {
        .pin_bit_mask = 1ULL << BSP_SD_POWER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    BSP_ERROR_CHECK_RETURN_ERR(gpio_config(&io_config));

    /*
     * SD_PWR_CTRL is active low.
     */
    return gpio_set_level(BSP_SD_POWER, enable ? 0 : 1);
}

/**************************************************************************************************
 * SPIFFS
 **************************************************************************************************/

esp_err_t bsp_spiffs_mount(void)
{
    const esp_vfs_spiffs_conf_t conf = {
        .base_path = BSP_SPIFFS_MOUNT_POINT,
        .partition_label = CONFIG_BSP_SPIFFS_PARTITION_LABEL,
        .max_files = CONFIG_BSP_SPIFFS_MAX_FILES,

#ifdef CONFIG_BSP_SPIFFS_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    BSP_ERROR_CHECK_RETURN_ERR(ret);

    size_t total = 0;
    size_t used = 0;

    ret = esp_spiffs_info(conf.partition_label, &total, &used);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to get SPIFFS partition information: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG,
             "SPIFFS mounted: total=%u bytes, used=%u bytes",
             (unsigned int)total,
             (unsigned int)used);

    return ESP_OK;
}

esp_err_t bsp_spiffs_unmount(void)
{
    return esp_vfs_spiffs_unregister(CONFIG_BSP_SPIFFS_PARTITION_LABEL);
}

/**************************************************************************************************
 * microSD
 **************************************************************************************************/

sdmmc_card_t *bsp_sdcard_get_handle(void)
{
    return s_bsp_sdcard;
}

void bsp_sdcard_get_sdmmc_host(const int slot, sdmmc_host_t *config)
{
    assert(config != NULL);

    /*
     * La LyraT Mini usa SDMMC; no SDSPI.
     *
     * SDMMC_HOST_DEFAULT() usa el host SDMMC estándar. El slot real se
     * define en bsp_sdcard_sdmmc_get_slot().
     */
    sdmmc_host_t host_config = SDMMC_HOST_DEFAULT();

    /*
     * El argumento slot se conserva por compatibilidad con la API
     * común de esp-bsp.
     */
    (void)slot;

    memcpy(config, &host_config, sizeof(sdmmc_host_t));
}

void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config)
{
    assert(config != NULL);

    (void)slot;

    memset(config, 0, sizeof(sdmmc_host_t));

    ESP_LOGE(TAG,
             "SDSPI mode is not supported by ESP32-LyraT-Mini BSP");
}

void bsp_sdcard_sdmmc_get_slot(const int slot,
                               sdmmc_slot_config_t *config)
{
    assert(config != NULL);

    /*
     * En ESP32, SDMMC_HOST_SLOT_0 usa pines asociados a la flash
     * interna y no es apropiado para la LyraT Mini.
     *
     * La placa usa señales en GPIO2, GPIO14 y GPIO15, por lo que se
     * emplea SLOT_1 con pines definidos explícitamente.
     */
    (void)slot;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    slot_config.width = 1;

    slot_config.clk = BSP_SD_CLK;
    slot_config.cmd = BSP_SD_CMD;
    slot_config.d0 = BSP_SD_D0;

    /*
     * Modo SDMMC de 1 bit: D1/D2/D3 no están conectados a la tarjeta.
     */
    slot_config.d1 = GPIO_NUM_NC;
    slot_config.d2 = GPIO_NUM_NC;
    slot_config.d3 = GPIO_NUM_NC;

    /*
     * GPIO34 es entrada únicamente, adecuado para detección de tarjeta.
     *
     * La polaridad exacta es gestionada por el driver SDMMC según la
     * configuración de la señal CD. Si en una revisión concreta falla
     * la detección, se puede usar SDMMC_SLOT_NO_CD temporalmente para
     * validar primero el bus SD.
     */
    slot_config.cd = BSP_SD_DET;
    slot_config.wp = SDMMC_SLOT_NO_WP;

    /*
     * Activa pull-ups internos donde estén disponibles. CMD y D0
     * disponen además de resistencias externas en el diseño.
     */
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    memcpy(config, &slot_config, sizeof(sdmmc_slot_config_t));
}

void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host,
                               sdspi_device_config_t *config)
{
    assert(config != NULL);

    (void)spi_host;

    memset(config, 0, sizeof(sdspi_device_config_t));

    ESP_LOGE(TAG,
             "SDSPI mode is not supported by ESP32-LyraT-Mini BSP");
}

esp_err_t bsp_sdcard_sdmmc_mount(bsp_sdcard_cfg_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_bsp_sdcard != NULL) {
        ESP_LOGW(TAG, "microSD card is already mounted");
        return ESP_OK;
    }

    sdmmc_host_t sd_host = {0};
    sdmmc_slot_config_t sd_slot = {0};

    const esp_vfs_fat_sdmmc_mount_config_t default_mount_config = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
    };

    if (cfg->mount == NULL) {
        cfg->mount = &default_mount_config;
    }

    if (cfg->host == NULL) {
        /*
         * SLOT_1 permite utilizar el pinout de la LyraT Mini:
         * GPIO14 / GPIO15 / GPIO2.
         */
        bsp_sdcard_get_sdmmc_host(SDMMC_HOST_SLOT_1, &sd_host);
        cfg->host = &sd_host;
    }

    if (cfg->slot.sdmmc == NULL) {
        bsp_sdcard_sdmmc_get_slot(SDMMC_HOST_SLOT_1, &sd_slot);
        cfg->slot.sdmmc = &sd_slot;
    }

#if !defined(CONFIG_FATFS_LONG_FILENAMES) || defined(CONFIG_FATFS_LFN_NONE)
    ESP_LOGW(TAG,
             "Long filenames are disabled. Enable FATFS long filenames "
             "in menuconfig if needed.");
#endif

    BSP_ERROR_CHECK_RETURN_ERR(bsp_sdcard_power_enable(true));

    /*
     * Tiempo para que se estabilice SDIO_3V3 después de habilitar Q9.
     * 10 ms es conservador y suficiente para la prueba inicial.
     */
    vTaskDelay(pdMS_TO_TICKS(10));

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(
                        BSP_SD_MOUNT_POINT,
                        cfg->host,
                        cfg->slot.sdmmc,
                        cfg->mount,
                        &s_bsp_sdcard
                    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to mount microSD card: %s",
                 esp_err_to_name(ret));

        s_bsp_sdcard = NULL;

        /*
         * Al fallar el montaje se apaga la tarjeta para no dejarla
         * alimentada innecesariamente.
         */
        bsp_sdcard_power_enable(false);

        return ret;
    }

    ESP_LOGI(TAG,
             "microSD mounted, card name: %s",
             s_bsp_sdcard->cid.name);

    return ESP_OK;
}

esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg)
{
    (void)cfg;

    ESP_LOGE(TAG,
             "SDSPI mode is not supported by ESP32-LyraT-Mini BSP");

    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_sdcard_mount(void)
{
    bsp_sdcard_cfg_t cfg = {0};

    return bsp_sdcard_sdmmc_mount(&cfg);
}

esp_err_t bsp_sdcard_unmount(void)
{
    if (s_bsp_sdcard == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(
                        BSP_SD_MOUNT_POINT,
                        s_bsp_sdcard
                    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to unmount microSD card: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    s_bsp_sdcard = NULL;

    /*
     * La tarjeta puede quedar apagada tras desmontarla. Si una aplicación
     * necesita acceso posterior, bsp_sdcard_mount() la vuelve a activar.
     */
    return bsp_sdcard_power_enable(false);
}