/*
 * ble_app_gatt.h
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#ifndef ASW_BLE_BLE_APP_GATT_H_
#define ASW_BLE_BLE_APP_GATT_H_

#include "host/ble_gap.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

int ble_app_gatt_init(void);
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
void gatt_svr_subscribe_cb(struct ble_gap_event *event);

#ifdef __cplusplus
}
#endif

#endif /* ASW_BLE_BLE_APP_GATT_H_ */
