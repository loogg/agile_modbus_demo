#include "drv_usart.h"
#include "init_module.h"
#include "agile_modbus.h"

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME    "rtu_master"
#define DBG_LEVEL           DBG_LOG
#include <rtdbg.h>

#define DEVICE_NAME         "usart2"

ALIGN(RT_ALIGN_SIZE)
/* 串口 */
static usr_device_t dev = RT_NULL;
static rt_uint8_t usart_send_buf[2048];
static rt_uint8_t usart_read_buf[256];
static struct rt_semaphore rx_sem;

/* modbus */
static rt_uint8_t ctx_send_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
static rt_uint8_t ctx_read_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
static rt_uint32_t send_count = 0;
static rt_uint32_t success_count = 0;

/* 线程 */
static rt_uint8_t _thread_stack[512];
static struct rt_thread _thread;


static int get_rtu_master_info(void)
{
    LOG_I("send_cnt:%u, success_cnt:%u", send_count, success_count);

    return RT_EOK;
}
MSH_CMD_EXPORT(get_rtu_master_info, get rtu master info);

static int _usart_receive(rt_uint8_t *read_buf, int read_bufsz, int timeout)
{
    rt_sem_control(&rx_sem, RT_IPC_CMD_RESET, RT_NULL);

    int len = 0;
    rt_uint8_t flag = 0;

    while(1)
    {
        int rc = usr_device_read(dev, 0, read_buf + len, read_bufsz);
        if(rc > 0)
        {
            len += rc;
            read_bufsz -= rc;
            if(read_bufsz <= 0)
                break;
            
            flag = 1;
        }
        else
        {
            if(flag)
                timeout = 20;
            
            if(rt_sem_take(&rx_sem, timeout) != RT_EOK)
                break;
        }
    }

    return len;
}

static int _usart_pass(rt_uint8_t *send_buf, int send_len, rt_uint8_t *read_buf, int read_bufsz, int timeout)
{
    if((send_buf == RT_NULL) || (send_len <= 0) || (read_buf == RT_NULL) || (read_bufsz <= 0) || (timeout <= 0))
        return -RT_ERROR;
    
    usr_device_control(dev, USR_DEVICE_USART_CMD_FLUSH, RT_NULL);
    usr_device_write(dev, 0, send_buf, send_len);
    int read_len = _usart_receive(read_buf, read_bufsz, timeout);

    return read_len;
}

static void rtu_master_entry(void *parameter)
{
    agile_modbus_rtu_t ctx;
    agile_modbus_rtu_init(&ctx, ctx_send_buf, sizeof(ctx_send_buf), ctx_read_buf, sizeof(ctx_read_buf));
    agile_modbus_set_slave(&(ctx._ctx), 1);

    rt_uint16_t hold_register[10];
    while(1)
    {
        rt_thread_mdelay(1000);

        send_count++;
        int send_len = agile_modbus_serialize_read_registers(&(ctx._ctx), 0, 10);
        int read_len = _usart_pass(ctx._ctx.send_buf, send_len, ctx._ctx.read_buf, ctx._ctx.read_bufsz, 1000);
        int rc = agile_modbus_deserialize_read_registers(&(ctx._ctx), read_len, hold_register);

        if(rc == 10)
        {
            success_count++;

            LOG_I("read %d hold registers", rc);
            
            rt_kprintf("values:\r\n");
            for (int i = 0; i < rc; i++)
            {
                rt_kprintf("Register %d: %02X\r\n", i, hold_register[i]);
            }
        }
    }
}

static rt_err_t rx_indicate(usr_device_t dev, rt_size_t size)
{
    rt_sem_release(&rx_sem);

    return RT_EOK;
}


static int rtu_master_init(void)
{
    dev = usr_device_find(DEVICE_NAME);
    if(dev == RT_NULL)
        return -RT_ERROR;
    
    rt_sem_init(&rx_sem, "rms_r", 0, RT_IPC_FLAG_FIFO);
    usr_device_set_rx_indicate(dev, rx_indicate);


    struct usr_device_usart_buffer buffer;
    buffer.send_buf = usart_send_buf;
    buffer.send_bufsz = sizeof(usart_send_buf);
    buffer.read_buf = usart_read_buf;
    buffer.read_bufsz = sizeof(usart_read_buf);
    usr_device_control(dev, USR_DEVICE_USART_CMD_SET_BUFFER, &buffer);
    usr_device_init(dev);

    rt_thread_init(&_thread,
                   "rtu_master",
                   rtu_master_entry,
                   RT_NULL,
                   &_thread_stack[0],
                   sizeof(_thread_stack),
                   3,
                   100);
    rt_thread_startup(&_thread);

    return RT_EOK;
}

static struct init_module rtu_master_init_module;

static int rtu_master_init_module_register(void)
{
    rtu_master_init_module.init = rtu_master_init;
    init_module_app_register(&rtu_master_init_module);

    return RT_EOK;
}
INIT_PREV_EXPORT(rtu_master_init_module_register);
