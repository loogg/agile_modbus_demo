#include "stm32f1xx_hal.h"
#include "usr-drivers.h"
#include <rthw.h>

#define driver_init     (drv->init)
#define driver_read     (drv->read)
#define driver_write    (drv->write)
#define driver_control  (drv->control)

static rt_slist_t usr_driver_header = RT_SLIST_OBJECT_INIT(usr_driver_header);

usr_driver_t usr_driver_find(const char *name)
{
    rt_slist_t *node;

    rt_base_t level = rt_hw_interrupt_disable();

    rt_slist_for_each(node, &usr_driver_header)
    {
        usr_driver_t drv = rt_slist_entry(node, struct usr_driver, slist);
        if(rt_strncmp(drv->name, name, RT_NAME_MAX) == 0)
        {
            rt_hw_interrupt_enable(level);

            return drv;
        }
    }

    rt_hw_interrupt_enable(level);

    return RT_NULL;
}

rt_err_t usr_driver_register(usr_driver_t drv, const char *name)
{
    if(usr_driver_find(name) != RT_NULL)
        return -RT_ERROR;
    
    rt_strncpy(drv->name, name, RT_NAME_MAX);

    rt_slist_init(&(drv->slist));

    rt_base_t level = rt_hw_interrupt_disable();

    rt_slist_append(&usr_driver_header, &(drv->slist));

    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

rt_err_t usr_driver_init(usr_driver_t drv)
{
    rt_err_t result = RT_EOK;

    RT_ASSERT(drv != RT_NULL);

    if(driver_init != RT_NULL)
        result = driver_init(drv);

    return result;
}

rt_size_t usr_driver_read(usr_driver_t drv, rt_off_t pos, void *buffer, rt_size_t size)
{
    RT_ASSERT(drv != RT_NULL);

    if(driver_read != RT_NULL)
        return driver_read(drv, pos, buffer, size);

    return 0;
}

rt_size_t usr_driver_write(usr_driver_t drv, rt_off_t pos, const void *buffer, rt_size_t size)
{
    RT_ASSERT(drv != RT_NULL);

    if(driver_write != RT_NULL)
        return driver_write(drv, pos, buffer, size);

    return 0;
}

rt_err_t usr_driver_control(usr_driver_t drv, int cmd, void *args)
{
    RT_ASSERT(drv != RT_NULL);

    if(driver_control != RT_NULL)
        return driver_control(drv, cmd, args);

    return -RT_ERROR;
}

rt_err_t usr_driver_set_rx_indicate(usr_driver_t drv, rt_err_t (*rx_indicate)(usr_driver_t drv, rt_size_t size))
{
    RT_ASSERT(drv != RT_NULL);

    rt_base_t level = rt_hw_interrupt_disable();

    drv->rx_indicate = rx_indicate;

    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

rt_err_t usr_driver_set_tx_complete(usr_driver_t drv, rt_err_t (*tx_complete)(usr_driver_t drv, void *buffer))
{
    RT_ASSERT(drv != RT_NULL);

    rt_base_t level = rt_hw_interrupt_disable();

    drv->tx_complete = tx_complete;

    rt_hw_interrupt_enable(level);

    return RT_EOK;
}
