/*
 * Task 13 : show "88" on two seven-segment digits at the same time.
 *
 * Display : Wokwi 7-segment, 2 digits, common cathode
 *           Shared segments A=GPIO13 B=GPIO12 C=GPIO14 D=GPIO27
 *                           E=GPIO26 F=GPIO25 G=GPIO33
 *           DIG1 (tens)  = GPIO16, active low
 *           DIG2 (units) = GPIO17, active low
 *
 * Because both digits show the same character, the two common pins can be
 * driven low together and all seven segments held high. This is a static
 * (non-multiplexed) drive. It is the direct preparation for Task 14-17,
 * where different digits force the firmware to switch DIG1/DIG2 rapidly.
 */

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SEG_A_GPIO        13
#define SEG_B_GPIO        12
#define SEG_C_GPIO        14
#define SEG_D_GPIO        27
#define SEG_E_GPIO        26
#define SEG_F_GPIO        25
#define SEG_G_GPIO        33
#define DIGIT_TENS_GPIO   16
#define DIGIT_UNITS_GPIO  17

#define SEGMENT_COUNT     7
#define PATTERN_8         0x7F  /* A..G all on for a common-cathode display */

static const char *TAG = "TASK13";

static const gpio_num_t s_segment_pins[SEGMENT_COUNT] = {
    SEG_A_GPIO, SEG_B_GPIO, SEG_C_GPIO, SEG_D_GPIO,
    SEG_E_GPIO, SEG_F_GPIO, SEG_G_GPIO,
};

static void display_show_88(void)
{
    /* Blank first: turn both common pins high (off). */
    gpio_set_level(DIGIT_TENS_GPIO, 1);
    gpio_set_level(DIGIT_UNITS_GPIO, 1);

    /* All segments on. */
    for (int i = 0; i < SEGMENT_COUNT; i++) {
        gpio_set_level(s_segment_pins[i], (PATTERN_8 >> i) & 0x01U);
    }

    /* Enable both digits simultaneously. */
    gpio_set_level(DIGIT_TENS_GPIO, 0);
    gpio_set_level(DIGIT_UNITS_GPIO, 0);
}

static void gpio_init(void)
{
    uint64_t output_mask = 0;
    for (int i = 0; i < SEGMENT_COUNT; i++) {
        output_mask |= 1ULL << s_segment_pins[i];
    }
    output_mask |= 1ULL << DIGIT_TENS_GPIO;
    output_mask |= 1ULL << DIGIT_UNITS_GPIO;

    const gpio_config_t io_config = {
        .pin_bit_mask = output_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));

    /* Safe state: all digits and segments off. */
    gpio_set_level(DIGIT_TENS_GPIO, 1);
    gpio_set_level(DIGIT_UNITS_GPIO, 1);
    for (int i = 0; i < SEGMENT_COUNT; i++) {
        gpio_set_level(s_segment_pins[i], 0);
    }
}

void app_main(void)
{
    gpio_init();
    display_show_88();

    ESP_LOGI(TAG, "Task 13: static drive, DIG1=GPIO%d and DIG2=GPIO%d both ON",
             DIGIT_TENS_GPIO, DIGIT_UNITS_GPIO);
    ESP_LOGI(TAG, "Task 13: pattern=0x%02X -> display shows 88", PATTERN_8);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
