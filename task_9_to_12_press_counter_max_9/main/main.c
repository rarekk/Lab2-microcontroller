/*
 * Task 9-12 : switch press counter, saturated at 9 (Submission Check 1).
 *
 * Display : Wokwi 7-segment, 1 digit, common cathode
 *           A=GPIO13  B=GPIO12  C=GPIO14  D=GPIO27
 *           E=GPIO26  F=GPIO25  G=GPIO33  COM=GND
 * Buttons : COUNT = GPIO18 (pull-up, active low, falling edge)
 *           RESET = GPIO19 (pull-up, active low, falling edge)
 *
 * The counter increases only while count < 9. Further presses are logged
 * and ignored, so the display never shows more than 9. RESET returns the
 * counter to 0 so the check can be repeated without rebooting.
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

#define BUTTON_COUNT_GPIO   18
#define BUTTON_RESET_GPIO   19
#define DEBOUNCE_US         50000
#define QUEUE_LENGTH        10
#define SEGMENT_COUNT       7
#define MAX_COUNT           9

static const char *TAG = "TASK9";

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
            if (s_count < MAX_COUNT) {
                s_count++;
                ESP_LOGI(TAG, "COUNT press -> %u (max %d)",
                         (unsigned)s_count, MAX_COUNT);
            } else {
                ESP_LOGW(TAG, "COUNT press ignored: already at max %d", MAX_COUNT);
            }
        } else if (gpio_num == BUTTON_RESET_GPIO) {
            s_count = 0;
            ESP_LOGI(TAG, "RESET press -> count=0");
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
    display_write_digit(0);

    s_button_queue = xQueueCreate(QUEUE_LENGTH, sizeof(uint32_t));
    configASSERT(s_button_queue != NULL);

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_COUNT_GPIO, button_isr,
                                         (void *)(uintptr_t)BUTTON_COUNT_GPIO));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_RESET_GPIO, button_isr,
                                         (void *)(uintptr_t)BUTTON_RESET_GPIO));

    BaseType_t created = xTaskCreate(button_task, "button_task", 3072, NULL, 10, NULL);
    configASSERT(created == pdPASS);

    ESP_LOGI(TAG, "Task 9-12: press GPIO%d to count, max=%d; GPIO%d resets.",
             BUTTON_COUNT_GPIO, MAX_COUNT, BUTTON_RESET_GPIO);

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
