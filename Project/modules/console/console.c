#include "stm32f1xx_hal.h"
#include "usr-drivers.h"

#define DEVICE_NAME         "usart1"

ALIGN(RT_ALIGN_SIZE)
/* 串口 */
static rt_uint8_t init_ok = 0;
static usr_driver_t drv = RT_NULL;
static rt_uint8_t usart_send_buf[5120];
static rt_uint8_t usart_read_buf[256];
static struct rt_semaphore rx_sem;

static rt_err_t rx_indicate(usr_driver_t drv, rt_size_t size)
{
    rt_sem_release(&rx_sem);

    return RT_EOK;
}

void rt_hw_console_output(const char *str)
{
    if(!init_ok)
        return;
    
    usr_driver_write(drv, 0, str, rt_strlen(str));
}

char rt_hw_console_getchar(void)
{
    while(!init_ok)
        rt_thread_mdelay(100);
    
    char ch = 0;

    while(1)
    {
        if(usr_driver_read(drv, 0, &ch, 1) == 1)
            break;
        
        rt_sem_control(&rx_sem, RT_IPC_CMD_RESET, RT_NULL);
        rt_sem_take(&rx_sem, rt_tick_from_millisecond(5000));
    }

    return ch;
}

int console_init(void)
{
    drv = usr_driver_find(DEVICE_NAME);
    if(drv == RT_NULL)
        return -RT_ERROR;
    
    rt_sem_init(&rx_sem, "con_rx", 0, RT_IPC_FLAG_FIFO);
    usr_driver_set_rx_indicate(drv, rx_indicate);

    struct usr_driver_usart_buffer buffer;
    buffer.send_buf = usart_send_buf;
    buffer.send_bufsz = sizeof(usart_send_buf);
    buffer.read_buf = usart_read_buf;
    buffer.read_bufsz = sizeof(usart_read_buf);
    usr_driver_control(drv, USR_DRIVER_USART_CMD_SET_BUFFER, &buffer);
    usr_driver_init(drv);

    init_ok = 1;

    return RT_EOK;
}
