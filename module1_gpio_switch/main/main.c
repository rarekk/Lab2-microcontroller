/*
 * Module 1 - Digital I/O, GPIO interrupt and ISR debounce
 *
 * Button: GPIO4 (input, internal pull-up, active low)
 * LED:    GPIO2 (output)
 *
 * A falling edge on the button triggers the ISR. The ISR applies a 50 ms
 * time-window debounce and posts the event to a FreeRTOS queue. A worker task
 * performs the actual LED toggle and logging, keeping the ISR short.
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

#define BUTTON_GPIO    4
#define LED_GPIO       2
#define DEBOUNCE_US    50000   /* 50 ms */
#define QUEUE_LENGTH   10

static const char *TAG = "MODULE1";

static QueueHandle_t s_button_queue;
static volatile int64_t s_last_isr_us;

/* Runs in interrupt context: keep it short and use only ...FromISR APIs. */
static void IRAM_ATTR button_isr(void *arg)
{
    const int64_t now_us = esp_timer_get_time();

    if (now_us - s_last_isr_us < DEBOUNCE_US) {
        return; /* Ignore contact bounce. */
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
    bool led_on = false;
    uint32_t press_count = 0;

    for (;;) {
        uint32_t gpio_num;
        if (xQueueReceive(s_button_queue, &gpio_num, portMAX_DELAY) == pdTRUE) {
            led_on = !led_on;
            gpio_set_level(LED_GPIO, led_on ? 1 : 0);
            press_count++;
            ESP_LOGI(TAG, "Button GPIO%" PRIu32 " pressed -> count=%" PRIu32 ", LED %s",
                     gpio_num, press_count, led_on ? "ON" : "OFF");
        }
    }
}

static void gpio_init(void)
{
    const gpio_config_t led_config = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_config));
    ESP_ERROR_CHECK(gpio_set_level(LED_GPIO, 0));

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

    /* Visual check that the output pin works before interrupts start. */
    for (int i = 0; i < 3; i++) {
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(120));
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    s_button_queue = xQueueCreate(QUEUE_LENGTH, sizeof(uint32_t));
    configASSERT(s_button_queue != NULL);

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_GPIO, button_isr,
                                         (void *)(uintptr_t)BUTTON_GPIO));

    BaseType_t created = xTaskCreate(button_task, "button_task", 3072, NULL, 10, NULL);
    configASSERT(created == pdPASS);

    ESP_LOGI(TAG, "Ready: press the button on GPIO%d (LED on GPIO%d).",
             BUTTON_GPIO, LED_GPIO);
}
