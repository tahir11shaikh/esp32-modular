/*
 * ble_app_gap.h
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#ifndef ASW_BLE_BLE_APP_GAP_H_
#define ASW_BLE_BLE_APP_GAP_H_

#ifdef __cplusplus
extern "C" {
#endif

#define DEVICE_NAME "ARM-N"

#define BLE_GAP_APPEARANCE_GENERIC_TAG 0x0200
#define BLE_GAP_URI_PREFIX_HTTPS 0x17
#define BLE_GAP_LE_ROLE_PERIPHERAL 0x00

int ble_app_gap_init(void);
void ble_gap_on_sync(void);
void ble_gap_on_reset(int reason);

#ifdef __cplusplus
}
#endif

#endif /* ASW_BLE_BLE_APP_GAP_H_ */
