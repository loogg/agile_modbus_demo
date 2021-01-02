#ifndef __DRV_GPIO_H
#define __DRV_GPIO_H
#include <rtthread.h>
#include "stm32f1xx_hal.h"
#include "usr_device.h"

#define PIN_LOW                         0x00
#define PIN_HIGH                        0x01

#define PIN_MODE_OUTPUT                 0x00
#define PIN_MODE_INPUT                  0x01
#define PIN_MODE_INPUT_PULLUP           0x02
#define PIN_MODE_INPUT_PULLDOWN         0x03
#define PIN_MODE_OUTPUT_OD              0x04

struct usr_device_pin
{
    struct usr_device parent;
};

struct usr_device_pin_mode
{
    rt_base_t pin;
    rt_base_t mode;
};

void drv_pin_mode(rt_base_t pin, rt_base_t mode);
void drv_pin_write(rt_base_t pin, rt_base_t value);
int drv_pin_read(rt_base_t pin);

#endif
