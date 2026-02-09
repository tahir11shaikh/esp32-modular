/*
 * mqtt_app.c
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#include "mqtt_app.h"
#include "gpio_app.h"

#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT_APP";
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static mqtt_app_status_t s_mqtt_status = MQTT_STS_IDLE;

/* ===== Internal event handler ===== */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
	case MQTT_EVENT_CONNECTED:
	    ESP_LOGI(TAG, "MQTT connected");
	    s_mqtt_status = MQTT_STS_CONNECTED;
	
	    // Subscribe to command topic
	    mqtt_app_subscribe("tahirshaikh/esp32/cmd", 0);
	
	    // Publish initial status
	    mqtt_app_publish("tahirshaikh/esp32/status", "ESP32 online", 0, false);
	    break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        s_mqtt_status = MQTT_STS_DISCONNECTED;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscribed to topic %d", event->msg_id);
        break;

	case MQTT_EVENT_DATA: {
	    ESP_LOGI(TAG, "Received topic=%.*s data=%.*s",
	             event->topic_len, event->topic,
	             event->data_len, event->data);
	
	    // Ensure topic is null-terminated
	    char topic[128] = {0};
	    memcpy(topic, event->topic, event->topic_len);
	
	    // Ensure data is null-terminated
	    char payload[128] = {0};
	    memcpy(payload, event->data, event->data_len);
	
	    if (strcmp(topic, "tahirshaikh/esp32/cmd") == 0) {
	        if (strcmp(payload, "LED_ON") == 0) {
	            gpio_app_set(1);
	            ESP_LOGI(TAG, "LED ON");
	        } else if (strcmp(payload, "LED_OFF") == 0) {
	            gpio_app_set(0);
	            ESP_LOGI(TAG, "LED OFF");
	        } else if (strcmp(payload, "LED_TOGGLE") == 0) {
	            gpio_app_toggle();
	            ESP_LOGI(TAG, "LED TOGGLE → %d", gpio_app_get());
	        }
	    }
	    break;
	}

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        s_mqtt_status = MQTT_STS_ERROR;
        break;

    default:
        ESP_LOGD(TAG, "Other MQTT event id:%d", event->event_id);
        break;
    }
}

/* ===== Public API ===== */
esp_err_t mqtt_app_init(void)
{
    s_mqtt_status = MQTT_STS_IDLE;
    return ESP_OK;
}

esp_err_t mqtt_app_start(void)
{
    if (s_mqtt_client) {
        ESP_LOGW(TAG, "MQTT client already running");
        return ESP_ERR_INVALID_STATE;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.hivemq.com:1883",
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(s_mqtt_client);
}

esp_err_t mqtt_app_publish(const char *topic, const char *payload, int qos, bool retain)
{
    if (!s_mqtt_client) return ESP_ERR_INVALID_STATE;
    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, qos, retain);
    ESP_LOGI(TAG, "Published msg id=%d topic=%s data=%s", msg_id, topic, payload);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_app_subscribe(const char *topic, int qos)
{
    if (!s_mqtt_client) return ESP_ERR_INVALID_STATE;
    int msg_id = esp_mqtt_client_subscribe(s_mqtt_client, topic, qos);
    ESP_LOGI(TAG, "Subscribe msg id=%d topic=%s qos=%d", msg_id, topic, qos);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_app_disconnect(void)
{
    if (!s_mqtt_client) return ESP_ERR_INVALID_STATE;
    esp_err_t err = esp_mqtt_client_stop(s_mqtt_client);
    esp_mqtt_client_destroy(s_mqtt_client);
    s_mqtt_client = NULL;
    s_mqtt_status = MQTT_STS_DISCONNECTED;
    return err;
}

mqtt_app_status_t mqtt_app_get_status(void)
{
    return s_mqtt_status;
}
