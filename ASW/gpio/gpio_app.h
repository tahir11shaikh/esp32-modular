/*
 * gpio_app.h
 *
 *  Created on: 20-Aug-2025
 *      Author: tahir
 */

#ifndef ASW_GPIO_GPIO_APP_H_
#define ASW_GPIO_GPIO_APP_H_

#ifdef __cplusplus
extern "C" {
#endif

void gpio_app_init(void);
void gpio_app_set(int level);
void gpio_app_toggle(void);
int  gpio_app_get(void);

#ifdef __cplusplus
}
#endif

#endif /* ASW_GPIO_GPIO_APP_H_ */