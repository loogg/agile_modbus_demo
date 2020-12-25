#ifndef __USR_DRIVERS_H
#define __USR_DRIVERS_H
#include <rtthread.h>

typedef struct usr_driver *usr_driver_t;

struct usr_driver
{
    char name[RT_NAME_MAX];

    /* driver call back */
    rt_err_t (*rx_indicate)(usr_driver_t drv, rt_size_t size);
    rt_err_t (*tx_complete)(usr_driver_t drv, void *buffer);

    /* common driver interface */
    rt_err_t (*init)(usr_driver_t drv);
    rt_size_t (*read)(usr_driver_t drv, rt_off_t pos, void *buffer, rt_size_t size);
    rt_size_t (*write)(usr_driver_t drv, rt_off_t pos, const void *buffer, rt_size_t size);
    rt_err_t (*control)(usr_driver_t drv, int cmd, void *args);

    rt_slist_t slist;
};


usr_driver_t usr_driver_find(const char *name);
rt_err_t usr_driver_register(usr_driver_t drv, const char *name);
rt_err_t usr_driver_init(usr_driver_t drv);
rt_size_t usr_driver_read(usr_driver_t drv, rt_off_t pos, void *buffer, rt_size_t size);
rt_size_t usr_driver_write(usr_driver_t drv, rt_off_t pos, const void *buffer, rt_size_t size);
rt_err_t usr_driver_control(usr_driver_t drv, int cmd, void *args);
rt_err_t usr_driver_set_rx_indicate(usr_driver_t drv, rt_err_t (*rx_indicate)(usr_driver_t drv, rt_size_t size));
rt_err_t usr_driver_set_tx_complete(usr_driver_t drv, rt_err_t (*tx_complete)(usr_driver_t drv, void *buffer));

#include "drv_gpio.h"
#include "drv_usart.h"

#endif
