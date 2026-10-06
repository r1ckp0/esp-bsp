#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp/esp32_lyrat_mini.h"

void app_main(void)
{
    led_indicator_handle_t leds[BSP_LED_NUM] = {0};
    int led_count = 0;

    ESP_ERROR_CHECK(
        bsp_led_indicator_create(
            leds,
            &led_count,
            BSP_LED_NUM
        )
    );

    ESP_ERROR_CHECK(bsp_led_set(leds[BSP_LED_GREEN], true));
    ESP_ERROR_CHECK(bsp_led_set(leds[BSP_LED_BLUE], false));

    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_ERROR_CHECK(bsp_led_set(leds[BSP_LED_GREEN], false));
    ESP_ERROR_CHECK(bsp_led_set(leds[BSP_LED_BLUE], true));

    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_ERROR_CHECK(
        led_indicator_start(
            leds[BSP_LED_GREEN],
            BSP_LED_BLINK_FAST
        )
    );

    ESP_ERROR_CHECK(
        led_indicator_start(
            leds[BSP_LED_BLUE],
            BSP_LED_BLINK_SLOW
        )
    );
}