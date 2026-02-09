/*
 * ble_app_gatt.c
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#include "ble_app_gatt.h"
#include "wifi_app.h"
#include "gpio_app.h"

#include "services/gatt/ble_svc_gatt.h"

/* ======== Characteristic User Description Descriptor UUIDs ======== */
#define UUID_CUD 			0x2901

/* ======== Custom Characteristic UUIDs ======== */
#define UUID_LED_CTRL		0x2F01
#define UUID_WIFI_PROV      0x2F02
#define UUID_OUTPUT_CTRL    0x2F03

#define UUID_LED_STATUS     0x2E01
#define UUID_WIFI_STATUS    0x2E02
#define UUID_SENSOR_DATA    0x2E03

static const char *TAG = "BLE_APP_GATT";

/* ==== Access Callbacks ==== */
static int smarthome_ctrl_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int smarthome_status_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

/* ==== UUID Definitions ==== */
/* Smart Home Control Service */
static const ble_uuid128_t smarthome_ctrl_svc_uuid =
    BLE_UUID128_INIT(0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef,
                     0xfe, 0xdc, 0xba, 0x09, 0x87, 0x65, 0x43, 0x21);

static const ble_uuid16_t led_ctrl_chr_uuid    = BLE_UUID16_INIT(UUID_LED_CTRL);
static const ble_uuid16_t wifi_prov_chr_uuid   = BLE_UUID16_INIT(UUID_WIFI_PROV);
static const ble_uuid16_t output_ctrl_chr_uuid = BLE_UUID16_INIT(UUID_OUTPUT_CTRL);

/* Smart Home Status Service */
static const ble_uuid128_t smarthome_status_svc_uuid =
    BLE_UUID128_INIT(0x22, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef,
                     0xfe, 0xdc, 0xba, 0x09, 0x87, 0x65, 0x43, 0x21);

static const ble_uuid16_t led_status_chr_uuid  = BLE_UUID16_INIT(UUID_LED_STATUS);
static const ble_uuid16_t wifi_status_chr_uuid = BLE_UUID16_INIT(UUID_WIFI_STATUS);
static const ble_uuid16_t sensor_data_chr_uuid = BLE_UUID16_INIT(UUID_SENSOR_DATA);

/* ==== Value Handles ==== */
/* Control Service */
static uint16_t led_ctrl_val_handle;
static uint16_t wifi_prov_val_handle;
static uint16_t output_ctrl_val_handle;

/* Status Service */
static uint16_t led_status_val_handle;
static uint16_t wifi_status_val_handle;
static uint16_t sensor_data_val_handle;

/* ==== Functions Handles ==== */
static int handle_led_control(const struct ble_gatt_access_ctxt *ctxt)
{
    if (ctxt->om->om_len != 1) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    uint8_t state = ctxt->om->om_data[0];
    ESP_LOGI(TAG, "LED control write: %s", state ? "ON" : "OFF");
    gpio_app_set(state);
    return 0;
}

// Expect "SSID:myssid;PASS:mypass"
static int handle_wifi_provisioning(const struct ble_gatt_access_ctxt *ctxt)
{
    char buf[96] = {0};
    int len = OS_MBUF_PKTLEN(ctxt->om);
    if (len > (int)sizeof(buf) - 1) len = sizeof(buf) - 1;
    ble_hs_mbuf_to_flat(ctxt->om, buf, len, NULL);

    char ssid[WIFI_SSID_MAX_LEN + 1] = {0};
    char pass[WIFI_PASS_MAX_LEN + 1] = {0};
    /* TODO: robust parsing; quick demo below */
    char *s = strstr(buf, "SSID:");
    char *p = strstr(buf, "PASS:");
    if (!s || !p) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    s += 5;
    p += 5;
    char *sc = strchr(s, ';');
    if (sc) *sc = '\0';

    strncpy(ssid, s, WIFI_SSID_MAX_LEN);
    strncpy(pass, p, WIFI_PASS_MAX_LEN);

    if (wifi_app_set_credentials(ssid, pass) == ESP_OK) {
        wifi_app_start();  // begin connect (first time) or reconnect
        ESP_LOGI(TAG, "Provisioned Wi-Fi: SSID='%s'", ssid);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int handle_output_control(const struct ble_gatt_access_ctxt *ctxt)
{
    if (ctxt->om->om_len != 1) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    uint8_t out_state = ctxt->om->om_data[0];
    ESP_LOGI(TAG, "Output state: %d", out_state);
    // control_output(out_state);
    return 0;
}

/* ==== GATT Descriptor Access Callback ==== */
static int gatt_dsc_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const char *desc = (const char *)arg;
    int rc = os_mbuf_append(ctxt->om, desc, strlen(desc));
    return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

/* Unified Smart Home Control Access Callback */
static int smarthome_ctrl_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGE(TAG, "Unexpected access op=%d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }

    // Get UUID of the characteristic
    const ble_uuid_t *uuid = ctxt->chr->uuid;
    if (uuid->type != BLE_UUID_TYPE_16) {
        ESP_LOGE(TAG, "Unsupported UUID type=%d", uuid->type);
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint16_t val = ((const ble_uuid16_t *)uuid)->value;

    switch (val)
    {
        case UUID_LED_CTRL:
            return handle_led_control(ctxt);

        case UUID_WIFI_PROV:
            return handle_wifi_provisioning(ctxt);

        case UUID_OUTPUT_CTRL:
            return handle_output_control(ctxt);

        default:
            ESP_LOGE(TAG, "Unknown characteristic UUID=0x%04X", val);
            return BLE_ATT_ERR_UNLIKELY;
    }
}

/* ==== Status Handlers (READ) ==== */
static int handle_led_status(struct ble_gatt_access_ctxt *ctxt)
{
    uint8_t led_state = 1; // led_is_on() ? 1 : 0;
    os_mbuf_append(ctxt->om, &led_state, sizeof(led_state));
    ESP_LOGI(TAG, "LED status read: %d", led_state);
    return 0;
}

static int handle_wifi_status(struct ble_gatt_access_ctxt *ctxt)
{
    uint8_t wifi_connected = 1; // wifi_is_connected() ? 1 : 0;
    os_mbuf_append(ctxt->om, &wifi_connected, sizeof(wifi_connected));
    ESP_LOGI(TAG, "Wi-Fi status read: %d", wifi_connected);
    return 0;
}

static int handle_sensor_data(struct ble_gatt_access_ctxt *ctxt)
{
    float temperature = 25.4; // get_temperature_sensor();
    os_mbuf_append(ctxt->om, &temperature, sizeof(temperature));
    ESP_LOGI(TAG, "Sensor data read: %.2f C", temperature);
    return 0;
}

static int smarthome_status_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid = ctxt->chr->uuid;
    if (uuid->type != BLE_UUID_TYPE_16) {
        ESP_LOGE(TAG, "Unsupported UUID type=%d", uuid->type);
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint16_t val = ((const ble_uuid16_t *)uuid)->value;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        ESP_LOGE(TAG, "Unexpected op=%d for UUID=0x%04X", ctxt->op, val);
        return BLE_ATT_ERR_UNLIKELY;
    }

	switch (val)
	{
	    case UUID_LED_STATUS: 
	        return handle_led_status(ctxt);
	
	    case UUID_WIFI_STATUS: 
	        return handle_wifi_status(ctxt);
	
	    case UUID_SENSOR_DATA: 
	        return handle_sensor_data(ctxt);
	
	    default:
	        ESP_LOGE(TAG, "Unknown status UUID=0x%04X", val);
	        return BLE_ATT_ERR_UNLIKELY;
	}
}

/* ==== GATT Services Table ==== */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {

    /* Smart Home Control Service */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &smarthome_ctrl_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {   /* LED Control (Write: ON/OFF) */
                .uuid = &led_ctrl_chr_uuid.u,
                .access_cb = smarthome_ctrl_access,
                .flags = BLE_GATT_CHR_F_WRITE,
                .val_handle = &led_ctrl_val_handle,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(UUID_CUD),
                        .att_flags = BLE_ATT_F_READ,
                        .access_cb = gatt_dsc_access,
                        .arg = "LED Control"
                    },
                    {0}
                }
            },
            {   /* Wi-Fi Provision (Write: SSID+Password) */
                .uuid = &wifi_prov_chr_uuid.u,
                .access_cb = smarthome_ctrl_access,
                .flags = BLE_GATT_CHR_F_WRITE,
                .val_handle = &wifi_prov_val_handle,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(UUID_CUD),
                        .att_flags = BLE_ATT_F_READ,
                        .access_cb = gatt_dsc_access,
                        .arg = "Wi-Fi Provision"
                    },
                    {0}
                }
            },
            {   /* Output Control (Write: generic output bits) */
                .uuid = &output_ctrl_chr_uuid.u,
                .access_cb = smarthome_ctrl_access,
                .flags = BLE_GATT_CHR_F_WRITE,
                .val_handle = &output_ctrl_val_handle,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(UUID_CUD),
                        .att_flags = BLE_ATT_F_READ,
                        .access_cb = gatt_dsc_access,
                        .arg = "Output Control"
                    },
                    {0}
                }
            },
            {0}
        }
    },

    /* Smart Home Status Service */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &smarthome_status_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {   /* LED Status (Indicate: ON/OFF) */
                .uuid = &led_status_chr_uuid.u,
                .access_cb = smarthome_status_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
                .val_handle = &led_status_val_handle,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(UUID_CUD),
                        .att_flags = BLE_ATT_F_READ,
                        .access_cb = gatt_dsc_access,
                        .arg = "LED Status"
                    },
                    {0}
                }
            },
            {   /* Wi-Fi Connection Status (Indicate) */
                .uuid = &wifi_status_chr_uuid.u,
                .access_cb = smarthome_status_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
                .val_handle = &wifi_status_val_handle,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(UUID_CUD),
                        .att_flags = BLE_ATT_F_READ,
                        .access_cb = gatt_dsc_access,
                        .arg = "Wi-Fi Status"
                    },
                    {0}
                }
            },
            {   /* Sensor Data (Indicate: temperature/humidity/etc.) */
                .uuid = &sensor_data_chr_uuid.u,
                .access_cb = smarthome_status_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
                .val_handle = &sensor_data_val_handle,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(UUID_CUD),
                        .att_flags = BLE_ATT_F_READ,
                        .access_cb = gatt_dsc_access,
                        .arg = "Sensor Data"
                    },
                    {0}
                }
            },
            {0}
        }
    },

    {0} /* No more services */
};

/*
// Public functions
void send_heart_rate_indication(void) {
    if (heart_rate_ind_status && heart_rate_chr_conn_handle_inited) {
        ble_gatts_indicate(heart_rate_chr_conn_handle,
                           heart_rate_chr_val_handle);
        ESP_LOGI(TAG, "heart rate indication sent!");
    }
}*/

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(DEBUG, "registering characteristic %s with "
                    "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

/*
 *  GATT server subscribe event callback
 *      1. Update heart rate subscription status
 */

void gatt_svr_subscribe_cb(struct ble_gap_event *event)
{
    /* Check connection handle */
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    } else {
        ESP_LOGI(TAG, "subscribe by nimble stack; attr_handle=%d",
                 event->subscribe.attr_handle);
    }

/*     Check attribute handle 
    if (event->subscribe.attr_handle == heart_rate_chr_val_handle) {
         Update heart rate subscription status 
        heart_rate_chr_conn_handle = event->subscribe.conn_handle;
        heart_rate_chr_conn_handle_inited = true;
        heart_rate_ind_status = event->subscribe.cur_indicate;
    }*/
}

int ble_app_gatt_init(void)
{
    /* Local variables */
    int rc;

    /* GATT service initialization */
    ble_svc_gatt_init();

    /* Update GATT services counter */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    /* Add GATT services */
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
