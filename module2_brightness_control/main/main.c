/*
 * Module 2 - ADC potentiometer input and LEDC PWM brightness control
 *
 * Potentiometer wiper: GPIO34 (ADC1 channel 6)
 * PWM LED:             GPIO2 (LEDC low-speed channel 0)
 *
 * The ADC is sampled 16 times and averaged to reduce noise. The 12-bit value
 * (0..4095) is linearly mapped to an 8-bit PWM duty (0..255) at 5 kHz.
 */

#include <inttypes.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_GPIO             2
#define POT_ADC_UNIT         ADC_UNIT_1
#define POT_ADC_CHANNEL      ADC_CHANNEL_6   /* GPIO34 on ESP32 */
#define PWM_FREQUENCY_HZ     5000
#define PWM_RESOLUTION       LEDC_TIMER_8_BIT
#define PWM_MAX_DUTY         255
#define ADC_MAX_RAW          4095
#define ADC_SAMPLES          16

static const char *TAG = "MODULE2";

static void pwm_init(void)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    const ledc_channel_config_t channel_config = {
        .gpio_num = LED_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags.output_invert = 0,
        .deconfigure = false,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

static void adc_init(adc_oneshot_unit_handle_t *adc_handle)
{
    const adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = POT_ADC_UNIT,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, adc_handle));

    const adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(*adc_handle, POT_ADC_CHANNEL,
                                               &channel_config));
}

void app_main(void)
{
    pwm_init();

    adc_oneshot_unit_handle_t adc_handle = NULL;
    adc_init(&adc_handle);

    ESP_LOGI(TAG, "Turn the potentiometer: ADC1_CH6 (GPIO34) -> PWM GPIO%d", LED_GPIO);

    for (;;) {
        int raw = 0;
        int sum = 0;

        for (int i = 0; i < ADC_SAMPLES; i++) {
            ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, POT_ADC_CHANNEL, &raw));
            sum += raw;
        }
        raw = sum / ADC_SAMPLES;

        const uint32_t duty = ((uint32_t)raw * PWM_MAX_DUTY) / ADC_MAX_RAW;
        const uint32_t millivolts = ((uint32_t)raw * 3300U) / ADC_MAX_RAW;

        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));

        ESP_LOGI(TAG, "ADC raw=%4d (%4" PRIu32 " mV) -> duty=%3" PRIu32 "/%d (%2" PRIu32 "%%)",
                 raw, millivolts, duty, PWM_MAX_DUTY, (duty * 100U) / PWM_MAX_DUTY);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
