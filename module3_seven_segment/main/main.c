/*
 * Module 3 - Multiplexed 2-digit seven-segment display + button interrupt
 *
 * Display : Wokwi 2-digit seven-segment, common cathode
 *           A=GPIO13 B=GPIO12 C=GPIO14 D=GPIO27 E=GPIO26 F=GPIO25 G=GPIO33
 *           DIG1 (tens)=GPIO16, DIG2 (units)=GPIO17 (both active low)
 * Button  : GPIO18 (input, internal pull-up, active low)
 *
 * The button ISR debounces in software and queues an event. A worker task
 * increments the count (00..99). app_main continuously multiplexes the two
 * digits by driving only one common pin low at a time.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define BUTTON_GPIO       18
#define DEBOUNCE_US       50000   /* 50 ms */
#define QUEUE_LENGTH      10
#define DIGIT_REFRESH_MS  2
#define DIGIT_COUNT       2
#define COUNT_MODULO      100

#define SEG_A_GPIO        13
#define SEG_B_GPIO        12
#define SEG_C_GPIO        14
#define SEG_D_GPIO        27
#define SEG_E_GPIO        26
#define SEG_F_GPIO        25
#define SEG_G_GPIO        33
#define DIGIT_TENS_GPIO   16
#define DIGIT_UNITS_GPIO  17

static const char *TAG = "MODULE3";

static const gpio_num_t s_segment_pins[7] = {
    SEG_A_GPIO, SEG_B_GPIO, SEG_C_GPIO, SEG_D_GPIO,
    SEG_E_GPIO, SEG_F_GPIO, SEG_G_GPIO,
};

static const gpio_num_t s_digit_pins[DIGIT_COUNT] = {
    DIGIT_TENS_GPIO, DIGIT_UNITS_GPIO,
};

/* Common cathode: bit0=A ... bit6=G, 1 means segment on. */
static const uint8_t s_digit_patterns[10] = {
    0x3F, /* 0 */
    0x06, /* 1 */
    0x5B, /* 2 */
    0x4F, /* 3 */
    0x66, /* 4 */
    0x6D, /* 5 */
    0x7D, /* 6 */
    0x07, /* 7 */
    0x7F, /* 8 */
    0x6F, /* 9 */
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
        uint32_t gpio_num;
        if (xQueueReceive(s_button_queue, &gpio_num, portMAX_DELAY) == pdTRUE) {
            s_count = (uint8_t)((s_count + 1U) % COUNT_MODULO);
            ESP_LOGI(TAG, "Button GPIO%" PRIu32 " pressed -> count=%02u",
                     gpio_num, (unsigned)s_count);
        }
    }
}

static void display_write_segments(uint8_t pattern)
{
    for (int i = 0; i < 7; i++) {
        gpio_set_level(s_segment_pins[i], (pattern >> i) & 0x01);
    }
}

static void display_digits_off(void)
{
    for (int i = 0; i < DIGIT_COUNT; i++) {
        gpio_set_level(s_digit_pins[i], 1); /* Common cathode off. */
    }
}

static void display_show_digit(int digit_index, uint8_t value)
{
    display_digits_off();
    display_write_segments(s_digit_patterns[value % 10]);
    gpio_set_level(s_digit_pins[digit_index], 0);
    vTaskDelay(pdMS_TO_TICKS(DIGIT_REFRESH_MS));
    gpio_set_level(s_digit_pins[digit_index], 1);
}

static void gpio_init(void)
{
    uint64_t output_mask = 0;
    for (int i = 0; i < 7; i++) {
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
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&button_config));
}

static void display_self_test(void)
{
    for (int i = 0; i < DIGIT_COUNT; i++) {
        display_digits_off();
        display_write_segments(s_digit_patterns[8]);
        gpio_set_level(s_digit_pins[i], 0);
        vTaskDelay(pdMS_TO_TICKS(400));
        display_digits_off();
    }
}

void app_main(void)
{
    gpio_init();
    display_self_test();

    s_button_queue = xQueueCreate(QUEUE_LENGTH, sizeof(uint32_t));
    configASSERT(s_button_queue != NULL);

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_GPIO, button_isr,
                                         (void *)(uintptr_t)BUTTON_GPIO));

    BaseType_t created = xTaskCreate(button_task, "button_task", 3072, NULL, 10, NULL);
    configASSERT(created == pdPASS);

    ESP_LOGI(TAG, "Ready: press GPIO%d to count. Display is multiplexed at ~250 Hz.",
             BUTTON_GPIO);

    for (;;) {
        const uint8_t value = s_count;
        display_show_digit(0, (uint8_t)(value / 10U));
        display_show_digit(1, (uint8_t)(value % 10U));
    }
}
