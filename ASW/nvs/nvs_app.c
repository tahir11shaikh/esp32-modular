/*
 * nvs_app.c
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#include "nvs_app.h"
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "NVS_APP";

void nvs_app_init(void)
{
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return;
    }
}

/* ===== NVS helpers ===== */
esp_err_t nvs_app_load_wifi_creds_from_nvs(wifi_app_creds_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvh;
    esp_err_t err = nvs_open(WIFI_NVS_NS, NVS_READONLY, &nvh);
    if (err != ESP_OK) return err;

    size_t ssid_len = WIFI_SSID_MAX_LEN;
    size_t pass_len = WIFI_PASS_MAX_LEN;
    memset(out, 0, sizeof(*out));

    err = nvs_get_str(nvh, WIFI_NVS_KEY_SSID, out->ssid, &ssid_len);
    if (err == ESP_OK) {
        err = nvs_get_str(nvh, WIFI_NVS_KEY_PASS, out->pass, &pass_len);
    }
    nvs_close(nvh);

    out->valid = (err == ESP_OK) && (out->ssid[0] != '\0');
    return err;
}

esp_err_t nvs_app_save_wifi_creds_to_nvs(const wifi_app_creds_t *in)
{
    nvs_handle_t nvh;
    nvs_open(WIFI_NVS_NS, NVS_READWRITE, &nvh);
    esp_err_t err = nvs_set_str(nvh, WIFI_NVS_KEY_SSID, in->ssid);
    if (err == ESP_OK) err = nvs_set_str(nvh, WIFI_NVS_KEY_PASS, in->pass);
    if (err == ESP_OK) err = nvs_commit(nvh);
    nvs_close(nvh);
    return err;
}
