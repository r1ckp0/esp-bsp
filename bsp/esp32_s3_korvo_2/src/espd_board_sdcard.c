/*
 * Strong symbol override of espd_bsp_sdcard_mount for ESP32-S3-Korvo-2.
 * Bypasses the default check for BSP_SD_DET (which triggers I/O expander errors
 * because BSP_SD_DET is GPIO_NUM_NC / -1) and directly mounts the SD card using
 * the board's native BSP API.
 */

#include "esp_err.h"
#include "bsp/esp-bsp.h"

esp_err_t espd_bsp_sdcard_mount(const char *mount_point)
{
    (void)mount_point;
    return bsp_sdcard_mount();
}
