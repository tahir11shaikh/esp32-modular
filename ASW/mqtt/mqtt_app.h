/*
 * mqtt_app.h
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#ifndef ASW_MQTT_MQTT_APP_H_
#define ASW_MQTT_MQTT_APP_H_

#ifdef __cplusplus
extern "C" {
#endif

#pragma once
#include <stdbool.h>
#include "esp_err.h"

/* ===== MQTT Status ===== */
typedef enum {
    MQTT_STS_IDLE = 0,
    MQTT_STS_CONNECTED,
    MQTT_STS_DISCONNECTED,
    MQTT_STS_ERROR,
} mqtt_app_status_t;

/* ===== API ===== */
esp_err_t mqtt_app_init(void);
esp_err_t mqtt_app_start(void);
esp_err_t mqtt_app_publish(const char *topic, const char *data, int qos, bool retain);
esp_err_t mqtt_app_subscribe(const char *topic, int qos);
esp_err_t mqtt_app_disconnect(void);
mqtt_app_status_t mqtt_app_get_status(void);

#ifdef __cplusplus
}
#endif

#endif /* ASW_MQTT_MQTT_APP_H_ */
