/*
 * gpio_app.c
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#include "gpio_app.h"
#include "driver/gpio.h"

#define GPIO_APP_LED    GPIO_NUM_2   // ESP32 on-board LED

void gpio_app_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << GPIO_APP_LED,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(GPIO_APP_LED, 0); // start LED OFF
}

void gpio_app_set(int level)
{
    gpio_set_level(GPIO_APP_LED, level);
}

void gpio_app_toggle(void)
{
    int state = gpio_get_level(GPIO_APP_LED);
    gpio_set_level(GPIO_APP_LED, !state);
}

int gpio_app_get(void)
{
    return gpio_get_level(GPIO_APP_LED);
}