#include "drv_usart.h"

ALIGN(RT_ALIGN_SIZE)
/* 串口 */
static rt_uint8_t init_ok = 0;
static usr_device_t dev = RT_NULL;
static rt_uint8_t usart_send_buf[2048];
static rt_uint8_t usart_read_buf[16];
static struct rt_semaphore rx_sem;

static rt_err_t rx_indicate(usr_device_t dev, rt_size_t size)
{
    rt_sem_release(&rx_sem);

    return RT_EOK;
}

void rt_hw_console_output(const char *str)
{
    if(!init_ok)
        return;
    
    usr_device_write(dev, 0, str, rt_strlen(str));
}

char rt_hw_console_getchar(void)
{
    while(!init_ok)
        rt_thread_mdelay(100);
    
    char ch = 0;

    while(1)
    {
        if(usr_device_read(dev, 0, &ch, 1) == 1)
            break;
        
        rt_sem_control(&rx_sem, RT_IPC_CMD_RESET, RT_NULL);
        rt_sem_take(&rx_sem, rt_tick_from_millisecond(5000));
    }

    return ch;
}

int console_init(void)
{
    dev = usr_device_find(RT_CONSOLE_DEVICE_NAME);
    if(dev == RT_NULL)
        return -RT_ERROR;
    
    rt_sem_init(&rx_sem, "con_rx", 0, RT_IPC_FLAG_FIFO);
    usr_device_set_rx_indicate(dev, rx_indicate);

    struct usr_device_usart_buffer buffer;
    buffer.send_buf = usart_send_buf;
    buffer.send_bufsz = sizeof(usart_send_buf);
    buffer.read_buf = usart_read_buf;
    buffer.read_bufsz = sizeof(usart_read_buf);
    usr_device_control(dev, USR_DEVICE_USART_CMD_SET_BUFFER, &buffer);
    usr_device_init(dev);

    init_ok = 1;

    return RT_EOK;
}
