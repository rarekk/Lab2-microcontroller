/*
 * Task 5-8 : switch-controlled 0..9 counter on a single 7-segment display.
 *
 * Display : Wokwi 7-segment, 1 digit, common cathode
 *           A=GPIO13  B=GPIO12  C=GPIO14  D=GPIO27
 *           E=GPIO26  F=GPIO25  G=GPIO33  COM=GND
 * Button  : GPIO18, internal pull-up, active low, falling edge interrupt
 *
 * Every valid press increments the digit and wraps 9 -> 0. The ISR applies
 * a 50 ms time-window debounce and posts the GPIO number to a FreeRTOS
 * queue. A worker task performs the increment so the ISR stays short.
 */

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define SEG_A_GPIO        13
#define SEG_B_GPIO        12
#define SEG_C_GPIO        14
#define SEG_D_GPIO        27
#define SEG_E_GPIO        26
#define SEG_F_GPIO        25
#define SEG_G_GPIO        33

#define BUTTON_GPIO       18
#define DEBOUNCE_US       50000   /* 50 ms contact-bounce window */
#define QUEUE_LENGTH      10
#define SEGMENT_COUNT     7
#define DIGIT_MODULO      10

static const char *TAG = "TASK5";

static const gpio_num_t s_segment_pins[SEGMENT_COUNT] = {
    SEG_A_GPIO, SEG_B_GPIO, SEG_C_GPIO, SEG_D_GPIO,
    SEG_E_GPIO, SEG_F_GPIO, SEG_G_GPIO,
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

    /* Ignore every edge that arrives inside the debounce window. */
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
        if (xQueueReceive(s_button_queue, &gpio_num, portMAX_DELAY) == pdTRUE) {
            s_count = (uint8_t)((s_count + 1U) % DIGIT_MODULO);
            ESP_LOGI(TAG, "press on GPIO%u -> count=%u",
                     (unsigned)gpio_num, (unsigned)s_count);
        }
    }
}

static void display_write_digit(uint8_t digit)
{
    const uint8_t pattern = s_digit_patterns[digit % 10U];

    for (int i = 0; i < SEGMENT_COUNT; i++) {
        gpio_set_level(s_segment_pins[i], (pattern >> i) & 0x01U);
    }
}

static void gpio_init(void)
{
    uint64_t output_mask = 0;
    for (int i = 0; i < SEGMENT_COUNT; i++) {
        output_mask |= 1ULL << s_segment_pins[i];
    }

    const gpio_config_t output_config = {
        .pin_bit_mask = output_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&output_config));

    for (int i = 0; i < SEGMENT_COUNT; i++) {
        gpio_set_level(s_segment_pins[i], 0);
    }

    const gpio_config_t button_config = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
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
    display_write_digit(0);

    s_button_queue = xQueueCreate(QUEUE_LENGTH, sizeof(uint32_t));
    configASSERT(s_button_queue != NULL);

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_GPIO, button_isr,
                                         (void *)(uintptr_t)BUTTON_GPIO));

    BaseType_t created = xTaskCreate(button_task, "button_task", 3072, NULL, 10, NULL);
    configASSERT(created == pdPASS);

    ESP_LOGI(TAG, "Task 5-8: press GPIO%d to count 0-9 (wraps at 9).", BUTTON_GPIO);

    uint8_t last_shown = 0xFF;
    for (;;) {
        const uint8_t value = s_count;
        if (value != last_shown) {
            display_write_digit(value);
            last_shown = value;
            ESP_LOGI(TAG, "display=%u", (unsigned)value);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
