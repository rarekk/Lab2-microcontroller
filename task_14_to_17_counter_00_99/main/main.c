/*
 * Task 14-17 : 00..99 counter on a multiplexed 2-digit display
 *              (Submission Check 2).
 *
 * Display : Wokwi 7-segment, 2 digits, common cathode
 *           Shared segments A=GPIO13 B=GPIO12 C=GPIO14 D=GPIO27
 *                           E=GPIO26 F=GPIO25 G=GPIO33
 *           DIG1 (tens)  = GPIO16, active low
 *           DIG2 (units) = GPIO17, active low
 * Buttons : COUNT = GPIO18, RESET = GPIO19 (pull-up, active low)
 *
 * Both digits share the same seven segment lines, so only one common pin
 * may be low at a time. app_main alternates DIG1 and DIG2 every 2 ms; the
 * eye integrates the two 50% duty phases into a steady two-digit number.
 * The button ISR debounces for 50 ms and queues the event; a worker task
 * increments 00..99 and wraps 99 -> 00.
 */

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define SEG_A_GPIO          13
#define SEG_B_GPIO          12
#define SEG_C_GPIO          14
#define SEG_D_GPIO          27
#define SEG_E_GPIO          26
#define SEG_F_GPIO          25
#define SEG_G_GPIO          33
#define DIGIT_TENS_GPIO     16
#define DIGIT_UNITS_GPIO    17

#define BUTTON_COUNT_GPIO   18
#define BUTTON_RESET_GPIO   19
#define DEBOUNCE_US         50000
#define QUEUE_LENGTH        10
#define SEGMENT_COUNT       7
#define DIGIT_COUNT         2
#define DIGIT_REFRESH_MS    2
#define COUNT_MODULO        100

static const char *TAG = "TASK14";

static const gpio_num_t s_segment_pins[SEGMENT_COUNT] = {
    SEG_A_GPIO, SEG_B_GPIO, SEG_C_GPIO, SEG_D_GPIO,
    SEG_E_GPIO, SEG_F_GPIO, SEG_G_GPIO,
};

static const gpio_num_t s_digit_pins[DIGIT_COUNT] = {
    DIGIT_TENS_GPIO, DIGIT_UNITS_GPIO,
};

/* Common cathode: bit0=A ... bit6=G, 1 = segment on. */
static const uint8_t s_digit_patterns[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

static QueueHandle_t s_button_queue;
static volatile int64_t s_last_isr_us;
static volatile uint8_t s_count;

static void IRAM_ATTR button_isr(void *arg)
{
    const int64_t now_us = esp_timer_get_time();

    if (now_us - s_last_isr_us < DEBOUNCE_US) {
        return;
    }
    s_last_isr_us = now_us;

    const uint32_t gpio_num = (uint32_t)(uintptr_t)arg;
    BaseType_t high_priority_task_woken = pdFALSE;

    xQueueSendFromISR(s_button_queue, &gpio_num, &high_priority_task_woken);
    if (high_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void button_task(void *arg)
{
    (void)arg;

    for (;;) {
        uint32_t gpio_num = 0;
        if (xQueueReceive(s_button_queue, &gpio_num, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (gpio_num == BUTTON_COUNT_GPIO) {
            s_count = (uint8_t)((s_count + 1U) % COUNT_MODULO);
            ESP_LOGI(TAG, "COUNT press -> %02u", (unsigned)s_count);
        } else if (gpio_num == BUTTON_RESET_GPIO) {
            s_count = 0;
            ESP_LOGI(TAG, "RESET press -> 00");
        }
    }
}

static void display_digits_off(void)
{
    for (int i = 0; i < DIGIT_COUNT; i++) {
        gpio_set_level(s_digit_pins[i], 1); /* Common cathode off. */
    }
}

static void display_write_segments(uint8_t pattern)
{
    for (int i = 0; i < SEGMENT_COUNT; i++) {
        gpio_set_level(s_segment_pins[i], (pattern >> i) & 0x01U);
    }
}

/* Light exactly one digit for DIGIT_REFRESH_MS, then blank the display. */
static void display_show_digit(int digit_index, uint8_t value)
{
    display_digits_off();              /* no ghosting while segments change */
    display_write_segments(s_digit_patterns[value % 10U]);
    gpio_set_level(s_digit_pins[digit_index], 0);
    vTaskDelay(pdMS_TO_TICKS(DIGIT_REFRESH_MS));
    gpio_set_level(s_digit_pins[digit_index], 1);
}

static void gpio_init(void)
{
    uint64_t output_mask = 0;
    for (int i = 0; i < SEGMENT_COUNT; i++) {
        output_mask |= 1ULL << s_segment_pins[i];
    }
    for (int i = 0; i < DIGIT_COUNT; i++) {
        output_mask |= 1ULL << s_digit_pins[i];
    }

    const gpio_config_t output_config = {
        .pin_bit_mask = output_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&output_config));

    display_digits_off();
    display_write_segments(0x00);

    const gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_COUNT_GPIO) | (1ULL << BUTTON_RESET_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&button_config));
}

void app_main(void)
{
    gpio_init();

    s_button_queue = xQueueCreate(QUEUE_LENGTH, sizeof(uint32_t));
    configASSERT(s_button_queue != NULL);

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_COUNT_GPIO, button_isr,
                                         (void *)(uintptr_t)BUTTON_COUNT_GPIO));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_RESET_GPIO, button_isr,
                                         (void *)(uintptr_t)BUTTON_RESET_GPIO));

    BaseType_t created = xTaskCreate(button_task, "button_task", 3072, NULL, 10, NULL);
    configASSERT(created == pdPASS);

    ESP_LOGI(TAG, "Task 14-17: press GPIO%d to count 00-99, GPIO%d resets.",
             BUTTON_COUNT_GPIO, BUTTON_RESET_GPIO);
    ESP_LOGI(TAG, "Multiplex: DIG1=GPIO%d, DIG2=GPIO%d, %d ms per digit (~%d Hz).",
             DIGIT_TENS_GPIO, DIGIT_UNITS_GPIO, DIGIT_REFRESH_MS,
             1000 / (DIGIT_REFRESH_MS * DIGIT_COUNT));

    for (;;) {
        const uint8_t value = s_count; /* snapshot: both digits same frame */
        display_show_digit(0, (uint8_t)(value / 10U));
        display_show_digit(1, (uint8_t)(value % 10U));
    }
}
