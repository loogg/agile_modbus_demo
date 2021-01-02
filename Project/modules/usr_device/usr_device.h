#ifndef __USR_DEVICE_H
#define __USR_DEVICE_H
#include <rtthread.h>

typedef struct usr_device *usr_device_t;

struct usr_device
{
    char name[RT_NAME_MAX];

    /* device call back */
    rt_err_t (*rx_indicate)(usr_device_t dev, rt_size_t size);
    rt_err_t (*tx_complete)(usr_device_t dev, void *buffer);

    /* common device interface */
    rt_err_t (*init)(usr_device_t dev);
    rt_size_t (*read)(usr_device_t dev, rt_off_t pos, void *buffer, rt_size_t size);
    rt_size_t (*write)(usr_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size);
    rt_err_t (*control)(usr_device_t dev, int cmd, void *args);

    rt_slist_t slist;
};


usr_device_t usr_device_find(const char *name);
rt_err_t usr_device_register(usr_device_t dev, const char *name);
rt_err_t usr_device_init(usr_device_t dev);
rt_size_t usr_device_read(usr_device_t dev, rt_off_t pos, void *buffer, rt_size_t size);
rt_size_t usr_device_write(usr_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size);
rt_err_t usr_device_control(usr_device_t dev, int cmd, void *args);
rt_err_t usr_device_set_rx_indicate(usr_device_t dev, rt_err_t (*rx_indicate)(usr_device_t dev, rt_size_t size));
rt_err_t usr_device_set_tx_complete(usr_device_t dev, rt_err_t (*tx_complete)(usr_device_t dev, void *buffer));

#endif
