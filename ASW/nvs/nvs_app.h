/*
 * nvs_app.h
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#ifndef ASW_NVS_NVS_APP_H_
#define ASW_NVS_NVS_APP_H_

#ifdef __cplusplus
extern "C" {
#endif

#pragma once
#include "wifi_app.h"

/* ===== NVS namespace/keys ===== */
#define WIFI_NVS_NS      "wifi"
#define WIFI_NVS_KEY_SSID "ssid"
#define WIFI_NVS_KEY_PASS "pass"

void nvs_app_init(void);
esp_err_t nvs_app_load_wifi_creds_from_nvs(wifi_app_creds_t *out);
esp_err_t nvs_app_save_wifi_creds_to_nvs(const wifi_app_creds_t *in);

#ifdef __cplusplus
}
#endif

#endif /* ASW_NVS_NVS_APP_H_ */
