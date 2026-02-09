/*
 * wifi_app.c
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#include "wifi_app.h"
#include "nvs_app.h"
#include "mqtt_app.h"

#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"

static const char *TAG = "WIFI_APP";

/* ===== Internal command queue ===== */
typedef enum {
    WIFI_CMD_START = 0,
    WIFI_CMD_APPLY_NEW_CREDS,   // apply newly saved creds & connect
    WIFI_CMD_CONNECT,
    WIFI_CMD_DISCONNECT,
} wifi_cmd_t;

typedef struct {
    wifi_cmd_t cmd;
} wifi_msg_t;

/* ===== Module statics ===== */
static EventGroupHandle_t s_wifi_event_group;
static QueueHandle_t      s_wifi_cmd_q;
static TaskHandle_t       s_wifi_task_hdl;
static wifi_app_status_t  s_status = WIFI_STS_IDLE;
static wifi_app_creds_t   s_creds = {0};

/* ===== Forward declarations ===== */
static void wifi_task(void *arg);
static void wifi_issue_cmd(wifi_cmd_t cmd);

/* ===== Event handlers ===== */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT)
	{
	    switch (event_id) 
		{
	    case WIFI_EVENT_STA_START:
	        esp_wifi_connect();
	        ESP_LOGI(TAG, "Wi-Fi started, connecting...");
	        break;
	
	    case WIFI_EVENT_STA_CONNECTED:
	        ESP_LOGI(TAG, "STA connected to AP");
	        //s_status = WIFI_STS_REMOTEAP_CONNECTED;
	        xEventGroupClearBits(s_wifi_event_group, WIFI_REMOTEAP_DISCONNECTED);
	        break;
	
	    case WIFI_EVENT_STA_DISCONNECTED:
	        {
	            wifi_event_sta_disconnected_t *ev = (wifi_event_sta_disconnected_t *)event_data;
	            ESP_LOGW(TAG, "STA disconnected: reason=%d", ev ? ev->reason : -1);
	            xEventGroupClearBits(s_wifi_event_group, WIFI_REMOTEAP_CONNECTED | WIFI_REMOTEAP_CONNECTING);
	            xEventGroupSetBits(s_wifi_event_group, WIFI_REMOTEAP_DISCONNECTED);
	            s_status = WIFI_STS_REMOTEAP_DISCONNECTED;
	
	            /* Autoretry policy: let the task decide; here we just signal state */
	            /* Optionally: wifi_issue_cmd(WIFI_CMD_CONNECT); */
	        }
	        break;
	
		case WIFI_EVENT_STA_STOP:
		    ESP_LOGI(TAG, "Wi-Fi stopped");
		    xEventGroupSetBits(s_wifi_event_group, WIFI_STOPPED_BIT);
		    break;
	
	    default:
	        break;
	    }
	}
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_REMOTEAP_CONNECTED);
        xEventGroupClearBits(s_wifi_event_group, WIFI_REMOTEAP_CONNECTING | WIFI_REMOTEAP_DISCONNECTED);
        s_status = WIFI_STS_REMOTEAP_CONNECTED;

		mqtt_app_start();
    }
}

/* ===== Public API ===== */
esp_err_t wifi_app_init(void)
{
    s_wifi_event_group = xEventGroupCreate();
	s_wifi_cmd_q = xQueueCreate(8, sizeof(wifi_msg_t));

    // Network stack + default handlers
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

    // Wi-Fi init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &ip_event_handler,
                                                        NULL, &instance_got_ip));

    // Load credentials from NVS
    if (nvs_app_load_wifi_creds_from_nvs(&s_creds) == ESP_OK && s_creds.valid) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_REMOTEAP_CREDS_AVAILABLE);
        ESP_LOGI(TAG, "NVS creds found: SSID='%s'", s_creds.ssid);

        // Immediately try connecting
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

        wifi_config_t wcfg = {0};
        strncpy((char *)wcfg.sta.ssid,     s_creds.ssid, sizeof(wcfg.sta.ssid) - 1);
        strncpy((char *)wcfg.sta.password, s_creds.pass, sizeof(wcfg.sta.password) - 1);
        wcfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        wcfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
        ESP_ERROR_CHECK(esp_wifi_start());   // triggers WIFI_EVENT_STA_START
        s_status = WIFI_STS_REMOTEAP_CONNECTING;
        xEventGroupSetBits(s_wifi_event_group, WIFI_REMOTEAP_CONNECTING);
    } else {
        ESP_LOGI(TAG, "No Wi-Fi creds in NVS, starting in idle scan-ready mode");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());   // STA idle, ready for scan/provisioning
        s_status = WIFI_STS_IDLE;
    }

    // Worker task (for BLE-triggered actions)
    xTaskCreatePinnedToCore(wifi_task, "wifi_task", 4096, NULL, 6,&s_wifi_task_hdl, tskNO_AFFINITY);

    return ESP_OK;
}

esp_err_t wifi_app_start(void)
{
    wifi_issue_cmd(WIFI_CMD_START);
    return ESP_OK;
}

esp_err_t wifi_app_connect(void)
{
    wifi_issue_cmd(WIFI_CMD_CONNECT);
    return ESP_OK;
}

esp_err_t wifi_app_disconnect(void)
{
    wifi_issue_cmd(WIFI_CMD_DISCONNECT);
    return ESP_OK;
}

esp_err_t wifi_app_set_credentials(const char *ssid, const char *pass)
{
    if (!ssid || !pass) return ESP_ERR_INVALID_ARG;

    memset(&s_creds, 0, sizeof(s_creds));
    strncpy(s_creds.ssid, ssid, WIFI_SSID_MAX_LEN);
    strncpy(s_creds.pass, pass, WIFI_PASS_MAX_LEN);
    s_creds.valid = (strlen(s_creds.ssid) > 0);

    nvs_app_save_wifi_creds_to_nvs(&s_creds);

    xEventGroupSetBits(s_wifi_event_group, WIFI_REMOTEAP_CREDS_AVAILABLE);
    wifi_issue_cmd(WIFI_CMD_APPLY_NEW_CREDS);
    return ESP_OK;
}

esp_err_t wifi_app_get_credentials(wifi_app_creds_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = s_creds;
    return ESP_OK;
}

/* ===== Internal helpers ===== */
static void wifi_issue_cmd(wifi_cmd_t cmd)
{
    wifi_msg_t msg = {.cmd = cmd};
    (void)xQueueSend(s_wifi_cmd_q, &msg, 0);
}

static void wifi_apply_config_and_connect(void)
{
    if (!s_creds.valid) {
        ESP_LOGW(TAG, "No valid creds; not connecting");
        return;
    }

    wifi_config_t wcfg = {0};
    strncpy((char *)wcfg.sta.ssid,     s_creds.ssid, sizeof(wcfg.sta.ssid) - 1);
    strncpy((char *)wcfg.sta.password, s_creds.pass, sizeof(wcfg.sta.password) - 1);
    wcfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wcfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED && err != ESP_ERR_WIFI_NOT_INIT) {
        ESP_ERROR_CHECK(err);
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED && err != ESP_ERR_WIFI_NOT_INIT) {
        ESP_ERROR_CHECK(err);
    }

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_STOPPED_BIT,
                                           pdTRUE, pdTRUE, pdMS_TO_TICKS(500));
    if (!(bits & WIFI_STOPPED_BIT)) {
        ESP_LOGW(TAG, "Wi-Fi stop timeout, continuing anyway");
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_start());  // STA start event will trigger connect

    s_status = WIFI_STS_REMOTEAP_CONNECTING;
    xEventGroupSetBits(s_wifi_event_group, WIFI_REMOTEAP_CONNECTING);
}

static void wifi_disconnect_now(void)
{
    (void)esp_wifi_disconnect();
    xEventGroupClearBits(s_wifi_event_group, WIFI_REMOTEAP_CONNECTED | WIFI_REMOTEAP_CONNECTING);
    xEventGroupSetBits(s_wifi_event_group, WIFI_REMOTEAP_DISCONNECTED);
    s_status = WIFI_STS_REMOTEAP_DISCONNECTED;
}

/* ===== The Wi-Fi worker task ===== */
static void wifi_task(void *arg)
{
    wifi_msg_t msg;

    for (;;) {
        if (xQueueReceive(s_wifi_cmd_q, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.cmd) {
            case WIFI_CMD_START:
                ESP_LOGI(TAG, "CMD: START");
                if (s_creds.valid) {
                    wifi_apply_config_and_connect();
                } else {
                    ESP_LOGI(TAG, "Waiting for credentials...");
                }
                break;

            case WIFI_CMD_APPLY_NEW_CREDS:
                ESP_LOGI(TAG, "CMD: APPLY_NEW_CREDS");
                wifi_apply_config_and_connect();
                break;

            case WIFI_CMD_CONNECT:
                ESP_LOGI(TAG, "CMD: CONNECT");
                wifi_apply_config_and_connect();
                break;

            case WIFI_CMD_DISCONNECT:
                ESP_LOGI(TAG, "CMD: DISCONNECT");
                wifi_disconnect_now();
                break;

            default:
                break;
            }
        }
    }
}
