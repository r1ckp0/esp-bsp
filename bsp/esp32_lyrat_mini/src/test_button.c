#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp/esp32_lyrat_mini.h"

static const char *TAG = "button_adc_test";

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_adc_initialize());

    adc_oneshot_unit_handle_t adc = bsp_adc_get_handle();

    const adc_oneshot_chan_cfg_t channel_cfg = {
        .atten = BSP_BUTTON_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            adc,
            BSP_BUTTON_ADC_CHANNEL,
            &channel_cfg
        )
    );

    while (true) {
        int raw = 0;

        ESP_ERROR_CHECK(
            adc_oneshot_read(
                adc,
                BSP_BUTTON_ADC_CHANNEL,
                &raw
            )
        );

        ESP_LOGI(TAG, "GPIO39 / ADC1_CH3 raw=%d", raw);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}