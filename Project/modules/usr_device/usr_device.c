#include "usr_device.h"
#include <rthw.h>

#define device_init     (dev->init)
#define device_read     (dev->read)
#define device_write    (dev->write)
#define device_control  (dev->control)

static rt_slist_t usr_device_header = RT_SLIST_OBJECT_INIT(usr_device_header);

usr_device_t usr_device_find(const char *name)
{
    rt_slist_t *node;

    rt_base_t level = rt_hw_interrupt_disable();

    rt_slist_for_each(node, &usr_device_header)
    {
        usr_device_t dev = rt_slist_entry(node, struct usr_device, slist);
        if(rt_strncmp(dev->name, name, RT_NAME_MAX) == 0)
        {
            rt_hw_interrupt_enable(level);

            return dev;
        }
    }

    rt_hw_interrupt_enable(level);

    return RT_NULL;
}

rt_err_t usr_device_register(usr_device_t dev, const char *name)
{
    if(usr_device_find(name) != RT_NULL)
        return -RT_ERROR;
    
    rt_strncpy(dev->name, name, RT_NAME_MAX);

    rt_slist_init(&(dev->slist));

    rt_base_t level = rt_hw_interrupt_disable();

    rt_slist_append(&usr_device_header, &(dev->slist));

    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

rt_err_t usr_device_init(usr_device_t dev)
{
    rt_err_t result = RT_EOK;

    RT_ASSERT(dev != RT_NULL);

    if(device_init != RT_NULL)
        result = device_init(dev);

    return result;
}

rt_size_t usr_device_read(usr_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    RT_ASSERT(dev != RT_NULL);

    if(device_read != RT_NULL)
        return device_read(dev, pos, buffer, size);

    return 0;
}

rt_size_t usr_device_write(usr_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    RT_ASSERT(dev != RT_NULL);

    if(device_write != RT_NULL)
        return device_write(dev, pos, buffer, size);

    return 0;
}

rt_err_t usr_device_control(usr_device_t dev, int cmd, void *args)
{
    RT_ASSERT(dev != RT_NULL);

    if(device_control != RT_NULL)
        return device_control(dev, cmd, args);

    return -RT_ERROR;
}

rt_err_t usr_device_set_rx_indicate(usr_device_t dev, rt_err_t (*rx_indicate)(usr_device_t dev, rt_size_t size))
{
    RT_ASSERT(dev != RT_NULL);

    rt_base_t level = rt_hw_interrupt_disable();

    dev->rx_indicate = rx_indicate;

    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

rt_err_t usr_device_set_tx_complete(usr_device_t dev, rt_err_t (*tx_complete)(usr_device_t dev, void *buffer))
{
    RT_ASSERT(dev != RT_NULL);

    rt_base_t level = rt_hw_interrupt_disable();

    dev->tx_complete = tx_complete;

    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

static int list_usr_device(void)
{
    rt_kprintf("-------------------------\r\n");

    rt_slist_t *node;
    rt_slist_for_each(node, &usr_device_header)
    {
        usr_device_t dev = rt_slist_entry(node, struct usr_device, slist);
        rt_kprintf("device: %-*.*s\r\n", RT_NAME_MAX, RT_NAME_MAX, dev->name);
    }

    return RT_EOK;
}
MSH_CMD_EXPORT(list_usr_device, list usr device);
