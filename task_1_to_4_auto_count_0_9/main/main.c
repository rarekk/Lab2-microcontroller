/*
 * Task 1-4 : automatic 0..9 counter on a single 7-segment display.
 *
 * Display : Wokwi 7-segment, 1 digit, common cathode
 *           A=GPIO13  B=GPIO12  C=GPIO14  D=GPIO27
 *           E=GPIO26  F=GPIO25  G=GPIO33  COM=GND
 *
 * The common-cathode display lights a segment when its GPIO is HIGH.
 * app_main writes the pattern for 0, waits 1000 ms, then moves to the
 * next digit. After 9 the sequence restarts at 0 forever.
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

#define SEGMENT_COUNT     7
#define COUNT_STEP_MS     1000
#define DIGIT_MAX         9

static const char *TAG = "TASK1";

static const gpio_num_t s_segment_pins[SEGMENT_COUNT] = {
    SEG_A_GPIO, SEG_B_GPIO, SEG_C_GPIO, SEG_D_GPIO,
    SEG_E_GPIO, SEG_F_GPIO, SEG_G_GPIO,
};

/*
 * Common cathode: bit0=A, bit1=B, ... bit6=G.
 * A set bit means the segment is ON.
 *   0 = 0x3F, 1 = 0x06, 2 = 0x5B, 3 = 0x4F, 4 = 0x66
 *   5 = 0x6D, 6 = 0x7D, 7 = 0x07, 8 = 0x7F, 9 = 0x6F
 */
static const uint8_t s_digit_patterns[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

/* Configure all seven segment lines as push-pull outputs, all off. */
static void segments_init(void)
{
    uint64_t output_mask = 0;
    for (int i = 0; i < SEGMENT_COUNT; i++) {
        output_mask |= 1ULL << s_segment_pins[i];
    }

    const gpio_config_t io_config = {
        .pin_bit_mask = output_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));

    for (int i = 0; i < SEGMENT_COUNT; i++) {
        gpio_set_level(s_segment_pins[i], 0);
    }
}

/* Write one decimal digit (0..9) to the display. */
static void display_write_digit(uint8_t digit)
{
    const uint8_t pattern = s_digit_patterns[digit % 10U];

    for (int i = 0; i < SEGMENT_COUNT; i++) {
        gpio_set_level(s_segment_pins[i], (pattern >> i) & 0x01U);
    }
}

void app_main(void)
{
    segments_init();
    ESP_LOGI(TAG, "Task 1-4: auto counter 0-9, %d ms per digit", COUNT_STEP_MS);

    for (;;) {
        for (uint8_t digit = 0; digit <= DIGIT_MAX; digit++) {
            display_write_digit(digit);
            ESP_LOGI(TAG, "digit=%u pattern=0x%02X",
                     (unsigned)digit, s_digit_patterns[digit]);
            vTaskDelay(pdMS_TO_TICKS(COUNT_STEP_MS));
        }
    }
}
