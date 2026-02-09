/*
 * wifi_app.h
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#ifndef ASW_WIFI_WIFI_APP_H_
#define ASW_WIFI_WIFI_APP_H_

#ifdef __cplusplus
extern "C" {
#endif

#pragma once
#include <stdbool.h>
#include "esp_err.h"

/* ===== Public constants ===== */
#define WIFI_SSID_MAX_LEN    32
#define WIFI_PASS_MAX_LEN    64

/* Event group bits */
#define WIFI_REMOTEAP_CONNECTED     	BIT0
#define WIFI_REMOTEAP_CONNECTING    	BIT1
#define WIFI_REMOTEAP_DISCONNECTED  	BIT2
#define WIFI_REMOTEAP_CREDS_AVAILABLE	BIT3
#define WIFI_STOPPED_BIT       BIT4

/* Status enum (optional helper) */
typedef enum {
    WIFI_STS_IDLE = 0,
    WIFI_STS_REMOTEAP_CONNECTED,
    WIFI_STS_REMOTEAP_CONNECTING,
    WIFI_STS_REMOTEAP_DISCONNECTED,
    WIFI_STS_ERROR
} wifi_app_status_t;

/* Credentials container */
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char pass[WIFI_PASS_MAX_LEN];
    bool valid;
} wifi_app_creds_t;

/* ===== Public API ===== */
esp_err_t wifi_app_init(void);                        // init module, create task, restore creds
esp_err_t wifi_app_start(void);                       // start station (if creds present, try connect)
esp_err_t wifi_app_connect(void);                     // (re)connect using stored creds
esp_err_t wifi_app_disconnect(void);                  // disconnect STA
esp_err_t wifi_app_set_credentials(const char *ssid, const char *pass); // save to NVS + stage connect
esp_err_t wifi_app_get_credentials(wifi_app_creds_t *out);              // read from NVS

#ifdef __cplusplus
}
#endif

#endif /* ASW_WIFI_WIFI_APP_H_ */
