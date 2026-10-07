set(SRC
    "src/bsp_adc.c"
    "src/bsp_audio.c"
    "src/bsp_button.c"
    "src/bsp_i2c.c"
    "src/bsp_led.c"
    "src/bsp_storage.c"
    "src/led_blink_defaults.c"
)

set(REQ
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_i2s
    esp_driver_sdmmc
    esp_driver_sdspi
    esp_adc
    spiffs
    fatfs
    vfs

    esp_codec_dev
    button
    led_indicator
)

idf_component_register(
    SRCS ${SRC}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES ${REQ}
)