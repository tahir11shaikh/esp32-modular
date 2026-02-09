/*
 * main.c
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#include "main.h"
#include "nvs_app.h"
#include "ble_app.h"
#include "wifi_app.h"
#include "mqtt_app.h"
#include "gpio_app.h"

void app_main(void)
{
    /* Initialize NVS — it is used to store PHY calibration data */
    nvs_app_init();

    /* Initialize GPIO */
    gpio_app_init();

    /* Initialize BLE */
    ble_app_init();

    /* Initialize WIFI */
    wifi_app_init();

    /* Initialize MQTT */
    mqtt_app_init();
}