/*
 * ble_app.c
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#include "ble_app.h"
#include "ble_app_gap.h"
#include "ble_app_gatt.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG = "BLE_APP";

void ble_store_config_init(void);

#if MYNEWT_VAL(BLE_POWER_CONTROL)
static void ble_app_power_control(uint16_t conn_handle)
{
    int rc;

    rc = ble_gap_read_remote_transmit_power_level(conn_handle, 0x01 );  // Attempting on LE 1M phy
    assert (rc == 0);

    rc = ble_gap_set_transmit_power_reporting_enable(conn_handle, 0x1, 0x1);
    assert (rc == 0);
}
#endif

static void nimble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");

    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

static void nimble_host_config_init(void)
{
    ble_hs_cfg.sync_cb = ble_gap_on_sync;
    ble_hs_cfg.reset_cb = ble_gap_on_reset;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
	#if CONFIG_BLE_SM_IO_CAP_DISP_YES_NO
	    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_YESNO;
	#elif CONFIG_BLE_SM_IO_CAP_DISP_ONLY
	    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;
	#elif CONFIG_BLE_SM_IO_CAP_KEYBOARD_ONLY
	    ble_hs_cfg.sm_io_cap = BLE_HS_IO_KEYBOARD_ONLY;
	#elif CONFIG_BLE_SM_IO_CAP_NO_IO
	    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
	#elif CONFIG_BLE_SM_IO_CAP_KEYBOARD_DISP
	    ble_hs_cfg.sm_io_cap = BLE_HS_IO_KEYBOARD_DISPLAY;
	#else
	    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;  
	#endif
	#ifdef CONFIG_EXAMPLE_BONDING
	    ble_hs_cfg.sm_bonding = 1;  
	
	    // Key distribution: distribute encryption keys securely
	    ble_hs_cfg.sm_our_key_dist  |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_SIGN;
	    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_SIGN;
	#endif

	#ifdef CONFIG_EXAMPLE_MITM
	    ble_hs_cfg.sm_mitm = 1;
	#else
	    ble_hs_cfg.sm_mitm = 0;
	#endif
	
	#ifdef CONFIG_EXAMPLE_USE_SC
	    ble_hs_cfg.sm_sc = 1;
	#else
	    ble_hs_cfg.sm_sc = 0;
	#endif

    /* Store host configuration */
    ble_store_config_init();
}

void ble_app_init(void)
{
	int rc;
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ", ret);
        return;
    }

    /* Initialize the NimBLE host configuration */
	nimble_host_config_init();

    /* GAP service initialization */
    rc = ble_app_gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return;
    }

    /* GATT server initialization */
    rc = ble_app_gatt_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GATT server, error code: %d", rc);
        return;
    }

    /* Start the task */
    nimble_port_freertos_init(nimble_host_task);
}
