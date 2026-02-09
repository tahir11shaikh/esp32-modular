/*
 * ble_app_gap.c
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#include "ble_app_gap.h"

#include "esp_random.h"
#include "host/ble_hs.h"
#include "esp_peripheral.h"
#include "services/gap/ble_svc_gap.h"

#if CONFIG_EXAMPLE_EXTENDED_ADV
static uint8_t ext_adv_pattern_1[] = {
    0x02, 0x01, 0x06,
    0x03, 0x03, 0xab, 0xcd,
    0x03, 0x03, 0x18, 0x11,
    0x11, 0X09, 'n', 'i', 'm', 'b', 'l', 'e', '-', 'b', 'l', 'e', 'p', 'r', 'p', 'h', '-', 'e',
};
#endif

static const char *TAG = "BLE_APP_GAP";

static int ble_gap_event(struct ble_gap_event *event, void *arg);

#if CONFIG_EXAMPLE_RANDOM_ADDR
static uint8_t own_addr_type = BLE_OWN_ADDR_RANDOM;
#else
static uint8_t own_addr_type;
#endif

#if NIMBLE_BLE_CONNECT
static uint32_t generate_random_passkey(void)
{
    uint32_t rand_val = esp_random();   // 32-bit random
    return (rand_val % 1000000);        // 6-digit number [000000 - 999999]
}

static void ble_gap_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    MODLOG_DFLT(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=", desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    MODLOG_DFLT(INFO, " our_id_addr_type=%d our_id_addr=", desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    MODLOG_DFLT(INFO, " peer_ota_addr_type=%d peer_ota_addr=", desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    MODLOG_DFLT(INFO, " peer_id_addr_type=%d peer_id_addr=", desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    MODLOG_DFLT(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d " "encrypted=%d authenticated=%d bonded=%d\n",
                desc->conn_itvl, desc->conn_latency, desc->supervision_timeout, desc->sec_state.encrypted, desc->sec_state.authenticated, desc->sec_state.bonded);
}
#endif

#if CONFIG_EXAMPLE_RANDOM_ADDR
static void ble_app_set_addr(void)
{
    ble_addr_t addr;
    int rc;

    /* generate new non-resolvable private address */
    rc = ble_hs_id_gen_rnd(0, &addr);
    assert(rc == 0);

    /* set generated address */
    rc = ble_hs_id_set_rnd(addr.val);

    assert(rc == 0);
}
#endif

#if CONFIG_EXAMPLE_EXTENDED_ADV
static void ext_ble_gap_advertise(void)
{
    struct ble_gap_ext_adv_params params;
    struct os_mbuf *data;
    uint8_t instance = 0;
    int rc;

    /* First check if any instance is already active */
    if(ble_gap_ext_adv_active(instance)) {
        return;
    }

    /* use defaults for non-set params */
    memset (&params, 0, sizeof(params));

    /* enable connectable advertising */
    params.connectable = 1;

    /* advertise using random addr */
    params.own_addr_type = BLE_OWN_ADDR_PUBLIC;

    params.primary_phy = BLE_HCI_LE_PHY_1M;
    params.secondary_phy = BLE_HCI_LE_PHY_2M;
    //params.tx_power = 127;
    params.sid = 1;

    params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MIN;

    /* configure instance 0 */
    rc = ble_gap_ext_adv_configure(instance, &params, NULL, ble_gap_event, NULL);
    assert (rc == 0);

    /* in this case only scan response is allowed */

    /* get mbuf for scan rsp data */
    data = os_msys_get_pkthdr(sizeof(ext_adv_pattern_1), 0);
    assert(data);

    /* fill mbuf with scan rsp data */
    rc = os_mbuf_append(data, ext_adv_pattern_1, sizeof(ext_adv_pattern_1));
    assert(rc == 0);

    rc = ble_gap_ext_adv_set_data(instance, data);
    assert (rc == 0);

    /* start advertising */
    rc = ble_gap_ext_adv_start(instance, 0, 0);
    assert (rc == 0);
}
#else
static void ble_gap_advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields adv_fields;
#if CONFIG_BT_NIMBLE_GAP_SERVICE
    const char *name;
#endif
    int rc;

    memset(&adv_fields, 0, sizeof adv_fields);

    /* Set advertising flags */
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* Set device name */
#if CONFIG_BT_NIMBLE_GAP_SERVICE
    name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;
#endif

    /* Set device TX power */
    adv_fields.tx_pwr_lvl_is_present = 1;
    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    /* Set device appearance */
    adv_fields.appearance = BLE_GAP_APPEARANCE_GENERIC_TAG;
    adv_fields.appearance_is_present = 1;

    /* Set device LE role */
    adv_fields.le_role = BLE_GAP_LE_ROLE_PERIPHERAL;
    adv_fields.le_role_is_present = 1;

    /* Set advertiement fields */
    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Start advertising */
    memset(&adv_params, 0, sizeof adv_params);

    /* Set non-connetable and general discoverable mode to be a beacon */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    /* Set advertising interval */
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(500);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(510);

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising started!");
}
#endif

int ble_app_gap_init(void)
{
    /* Local variables */
    int rc = 0;

    /* Call NimBLE GAP initialization API */
    ble_svc_gap_init();

    /* Set GAP device name */
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device name to %s, error code: %d", DEVICE_NAME, rc);
        return rc;
    }
    return rc;
}

void ble_gap_on_sync(void)
{
    int rc;

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    assert(rc == 0);

    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    MODLOG_DFLT(INFO, "Device Address: ");
    print_addr(addr_val);
    MODLOG_DFLT(INFO, "\n");

    /* Begin advertising */
    ble_gap_advertise();
}

void ble_gap_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
#if NIMBLE_BLE_CONNECT
    struct ble_gap_conn_desc desc;
    int rc;
#endif

    switch (event->type) {

#if NIMBLE_BLE_CONNECT
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        MODLOG_DFLT(INFO, "connection %s; status=%d ", event->connect.status == 0 ? "established" : "failed", event->connect.status);

        if (event->connect.status == 0) {
	        // Request security (triggers pairing)
	        int rc = ble_gap_security_initiate(event->connect.conn_handle);
	        if (rc != 0) {
	            MODLOG_DFLT(ERROR, "ble_gap_security_initiate rc=%d\n", rc);
	        }
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            ble_gap_print_conn_desc(&desc);
        }
        MODLOG_DFLT(INFO, "\n");

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
		#if CONFIG_EXAMPLE_EXTENDED_ADV
			ext_ble_gap_advertise();
		#else
			ble_gap_advertise();
		#endif
        }

		#if MYNEWT_VAL(BLE_POWER_CONTROL)
			ble_app_power_control(event->connect.conn_handle);
		#endif
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        ble_gap_print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        /* Connection terminated; resume advertising. */
		#if CONFIG_EXAMPLE_EXTENDED_ADV
			ext_ble_gap_advertise();
		#else
			ble_gap_advertise();
		#endif
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        MODLOG_DFLT(INFO, "connection updated; status=%d ", event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        ble_gap_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "advertise complete; reason=%d", event->adv_complete.reason);
		#if CONFIG_EXAMPLE_EXTENDED_ADV
			ext_ble_gap_advertise();
		#else
			ble_gap_advertise();
		#endif
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        MODLOG_DFLT(INFO, "encryption change event; status=%d ", event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        ble_gap_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_NOTIFY_TX:
        MODLOG_DFLT(INFO, "notify_tx event; conn_handle=%d attr_handle=%d " "status=%d is_indication=%d",
                    event->notify_tx.conn_handle, event->notify_tx.attr_handle, event->notify_tx.status, event->notify_tx.indication);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "subscribe event; conn_handle=%d attr_handle=%d " "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                    event->subscribe.conn_handle, event->subscribe.attr_handle, event->subscribe.reason, event->subscribe.prev_notify,
                    event->subscribe.cur_notify, event->subscribe.prev_indicate, event->subscribe.cur_indicate);
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n", event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;

	case BLE_GAP_EVENT_PASSKEY_ACTION:
	    ESP_LOGI(TAG, "PASSKEY_ACTION_EVENT started");
	    struct ble_sm_io pkey = {0};
	    int rc;

	    switch (event->passkey.params.action) {
	        case BLE_SM_IOACT_DISP: {
	            // Generate new random passkey
	            uint32_t passkey = generate_random_passkey();
	            pkey.action = BLE_SM_IOACT_DISP;
	            pkey.passkey = passkey;

	            ESP_LOGI(TAG, "Generated Passkey: %06" PRIu32, passkey);
	            ESP_LOGI(TAG, "Enter this passkey on the mobile device to pair.");

	            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
	            ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
	            break;
	        }

	        case BLE_SM_IOACT_NUMCMP: {
	            ESP_LOGI(TAG, "Passkey shown on both devices: %" PRIu32, event->passkey.params.numcmp);

	            // For simplicity auto-accept numeric comparison
	            pkey.action = BLE_SM_IOACT_NUMCMP;
	            pkey.numcmp_accept = 1; // accept always

	            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
	            ESP_LOGI(TAG, "Auto-accepted NUMCMP, result: %d", rc);
	            break;
	        }

	        case BLE_SM_IOACT_INPUT: {
	            ESP_LOGI(TAG, "Mobile should display a passkey. Enter it on ESP side is not needed (auto reject).");
	            // In your case you don't want ESP to enter anything, skip or set to 0
	            pkey.action = BLE_SM_IOACT_INPUT;
	            pkey.passkey = 0; // not used
	            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
	            break;
	        }

	        case BLE_SM_IOACT_OOB: {
	            uint8_t dummy_oob[16] = {0};
	            pkey.action = BLE_SM_IOACT_OOB;
	            memcpy(pkey.oob, dummy_oob, sizeof(dummy_oob));

	            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
	            ESP_LOGI(TAG, "Provided dummy OOB, result: %d", rc);
	            break;
	        }

	        default:
	            ESP_LOGW(TAG, "Unhandled passkey action: %d", event->passkey.params.action);
	            break;
	    }
	    return 0;

    case BLE_GAP_EVENT_AUTHORIZE:
        MODLOG_DFLT(INFO, "authorize event: conn_handle=%d attr_handle=%d is_read=%d", event->authorize.conn_handle, event->authorize.attr_handle, event->authorize.is_read);

        /* The default behaviour for the event is to reject authorize request */
        event->authorize.out_response = BLE_GAP_AUTHORIZE_REJECT;
        return 0;

	#if MYNEWT_VAL(BLE_POWER_CONTROL)
    case BLE_GAP_EVENT_TRANSMIT_POWER:
        MODLOG_DFLT(INFO, "Transmit power event : status=%d conn_handle=%d reason=%d " "phy=%d power_level=%x power_level_flag=%d delta=%d",
                    event->transmit_power.status,
                    event->transmit_power.conn_handle,
                    event->transmit_power.reason,
                    event->transmit_power.phy,
                    event->transmit_power.transmit_power_level,
                    event->transmit_power.transmit_power_level_flag,
                    event->transmit_power.delta);
        return 0;

    case BLE_GAP_EVENT_PATHLOSS_THRESHOLD:
        MODLOG_DFLT(INFO, "Pathloss threshold event : conn_handle=%d current path loss=%d " "zone_entered =%d",
                    event->pathloss_threshold.conn_handle, event->pathloss_threshold.current_path_loss, event->pathloss_threshold.zone_entered);
        return 0;
	#endif

	#if MYNEWT_VAL(BLE_EATT_CHAN_NUM) > 0
    case BLE_GAP_EVENT_EATT:
        MODLOG_DFLT(INFO, "EATT %s : conn_handle=%d cid=%d", event->eatt.status ? "disconnected" : "connected", event->eatt.conn_handle, event->eatt.cid);
		if (event->eatt.status) {
			/* Abort if disconnected */
			return 0;
		}
		cids[bearers] = event->eatt.cid;
		bearers += 1;
		if (bearers != MYNEWT_VAL(BLE_EATT_CHAN_NUM)) {
			/* Wait until all EATT bearers are connected before proceeding */
			return 0;
		}
		/* Set the default bearer to use for further procedures */
		rc = ble_att_set_default_bearer_using_cid(event->eatt.conn_handle, cids[0]);
		if (rc != 0) {
			MODLOG_DFLT(INFO, "Cannot set default EATT bearer, rc = %d\n", rc);
			return rc;
		}
		return 0;
	#endif

	#if MYNEWT_VAL(BLE_CONN_SUBRATING)
    case BLE_GAP_EVENT_SUBRATE_CHANGE:
        MODLOG_DFLT(INFO, "Subrate change event : conn_handle=%d status=%d factor=%d", event->subrate_change.conn_handle, event->subrate_change.status, event->subrate_change.subrate_factor);
        return 0;
	#endif
#endif
    }
    return 0;
}
