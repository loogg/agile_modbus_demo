#include "drv_usart.h"
#include "drv_usart_config.h"
#include "drv_gpio.h"
#include <rthw.h>

#define DRV_USART_RS485_INIT()                                                                      \
    {                                                                                               \
        if((usart->config->rs485_control_pin >= 0) && (usart->config->rs485_send_logic >= 0))       \
        {                                                                                           \
            drv_pin_mode(usart->config->rs485_control_pin, PIN_MODE_OUTPUT);                        \
        }                                                                                           \
    }

#define DRV_USART_RS485_RECV()                                                                      \
    {                                                                                               \
        if((usart->config->rs485_control_pin >= 0) && (usart->config->rs485_send_logic >= 0))       \
        {                                                                                           \
            drv_pin_write(usart->config->rs485_control_pin, !(usart->config->rs485_send_logic));    \
        }                                                                                           \
    }

#define DRV_USART_RS485_SEND()                                                                      \
    {                                                                                               \
        if((usart->config->rs485_control_pin >= 0) && (usart->config->rs485_send_logic >= 0))       \
        {                                                                                           \
            drv_pin_write(usart->config->rs485_control_pin, usart->config->rs485_send_logic);       \
        }                                                                                           \
    }

static rt_slist_t drv_usart_header = RT_SLIST_OBJECT_INIT(drv_usart_header);

static struct usr_device_usart_config usart_config[] =
{
    USART1_CONFIG,
    USART2_CONFIG,
    USART3_CONFIG
};

static struct usr_device_usart usart_obj[sizeof(usart_config) / sizeof(usart_config[0])] = {0};

static rt_err_t _usart_init(usr_device_t dev)
{
    RT_ASSERT(dev != RT_NULL);
    struct usr_device_usart *usart = (struct usr_device_usart *)dev;

    if((usart->buffer.send_buf == RT_NULL) || (usart->buffer.send_bufsz < RT_ALIGN_SIZE) ||
       (usart->buffer.read_buf == RT_NULL) || (usart->buffer.read_bufsz < RT_ALIGN_SIZE))
        return -RT_ERROR;

    rt_base_t level = rt_hw_interrupt_disable();

    if(usart->init_ok)
        HAL_UART_DeInit(usart->config->handle);
    
    usart->error_cnt = 0;
    usart->reset_flag = 0;
    rt_ringbuffer_init(&(usart->tx_rb), usart->buffer.send_buf, usart->buffer.send_bufsz);
    rt_ringbuffer_init(&(usart->rx_rb), usart->buffer.read_buf, usart->buffer.read_bufsz);
    usart->tx_activated = RT_FALSE;
    usart->tx_activated_timeout = rt_tick_get();
    usart->rx_timeout = rt_tick_get() + rt_tick_from_millisecond(USR_DEVICE_USART_RX_TIMEOUT * 1000);
    DRV_USART_RS485_INIT();
    DRV_USART_RS485_RECV();

    usart->config->handle->Instance = usart->config->instance;
    usart->config->handle->Init.BaudRate = usart->parameter.baudrate;
    usart->config->handle->Init.WordLength = usart->parameter.wlen;
    usart->config->handle->Init.StopBits = usart->parameter.stblen;
    usart->config->handle->Init.Parity = usart->parameter.parity;
    usart->config->handle->Init.Mode = UART_MODE_TX_RX;
    usart->config->handle->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    usart->config->handle->Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(usart->config->handle);
    HAL_UART_Abort(usart->config->handle);
    HAL_UART_Receive_IT(usart->config->handle, &(usart->ignore_data), 1);

    usart->init_ok = 1;

    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

static rt_size_t _usart_read(usr_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    RT_ASSERT(dev != RT_NULL);
    struct usr_device_usart *usart = (struct usr_device_usart *)dev;

    if(!usart->init_ok)
        return 0;
    if(size <= 0)
        return size;
    
    if(size > RT_UINT16_MAX)
        size = RT_UINT16_MAX;
    
    rt_size_t len = 0;

    rt_base_t level = rt_hw_interrupt_disable();
    len = rt_ringbuffer_get(&(usart->rx_rb), buffer, size);
    rt_hw_interrupt_enable(level);

    return len;
}

static rt_size_t _usart_write(usr_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    RT_ASSERT(dev != RT_NULL);
    struct usr_device_usart *usart = (struct usr_device_usart *)dev;

    if(!usart->init_ok)
        return 0;
    if(usart->error_cnt >= USR_DEVICE_USART_MAX_ERROR_CNT)
        return 0;
    if(size <= 0)
        return size;
    
    int put_len = 0;

    rt_base_t level = rt_hw_interrupt_disable();
    put_len = rt_ringbuffer_put(&(usart->tx_rb), buffer, size);
    do
    {
        if(usart->tx_activated == RT_TRUE)
            break;
        
        rt_uint8_t *send_ptr = RT_NULL;
        int send_len = rt_ringbuffer_peak(&(usart->tx_rb), &send_ptr);
        if(send_len <= 0)
            break;
        
        DRV_USART_RS485_SEND();
        usart->tx_activated = RT_TRUE;
        HAL_UART_AbortTransmit(usart->config->handle);
        HAL_UART_Transmit_DMA(usart->config->handle, send_ptr, send_len);
        usart->tx_activated_timeout = rt_tick_get() + rt_tick_from_millisecond(USR_DEVICE_USART_TX_ACTIVATED_TIMEOUT * 1000);
    }while(0);
    rt_hw_interrupt_enable(level);

    return put_len;
}

static rt_err_t _usart_control(usr_device_t dev, int cmd, void *args)
{
    RT_ASSERT(dev != RT_NULL);
    struct usr_device_usart *usart = (struct usr_device_usart *)dev;

    rt_err_t result = -RT_ERROR;

    switch(cmd)
    {
        case USR_DEVICE_USART_CMD_SET_PARAMETER:
        {
            struct usr_device_usart_parameter *parameter = args;
            if(parameter == RT_NULL)
                break;

            if(!IS_UART_BAUDRATE(parameter->baudrate))
                break;
            if(!IS_UART_PARITY(parameter->parity))
                break;
            if(!IS_UART_WORD_LENGTH(parameter->wlen))
                break;
            if(!IS_UART_STOPBITS(parameter->stblen))
                break;
            
            rt_base_t level = rt_hw_interrupt_disable();
            if(usart->init_ok)
                HAL_UART_Abort(usart->config->handle);
            usart->parameter = *parameter;
            usart->reset_flag = 1;
            rt_hw_interrupt_enable(level);

            result = RT_EOK;
        }
        break;

        case USR_DEVICE_USART_CMD_SET_BUFFER:
        {
            struct usr_device_usart_buffer *buffer = args;
            if(buffer == RT_NULL)
                break;
            
            if(buffer->send_buf == RT_NULL)
                break;
            if(buffer->send_bufsz < RT_ALIGN_SIZE)
                break;
            if(buffer->read_buf == RT_NULL)
                break;
            if(buffer->read_bufsz < RT_ALIGN_SIZE)
                break;
            
            rt_base_t level = rt_hw_interrupt_disable();
            if(usart->init_ok)
                HAL_UART_Abort(usart->config->handle);
            usart->buffer = *buffer;
            usart->reset_flag = 1;
            rt_hw_interrupt_enable(level);

            result = RT_EOK;
        }
        break;

        case USR_DEVICE_USART_CMD_FLUSH:
        {
            if(!usart->init_ok)
            {
                result = RT_EOK;
                break;
            }

            rt_base_t level = rt_hw_interrupt_disable();
            HAL_UART_Abort(usart->config->handle);
            rt_ringbuffer_reset(&(usart->tx_rb));
            rt_ringbuffer_reset(&(usart->rx_rb));
            usart->tx_activated = RT_FALSE;
            usart->tx_activated_timeout = rt_tick_get();
            DRV_USART_RS485_RECV();
            HAL_UART_Receive_IT(usart->config->handle, &(usart->ignore_data), 1);
            rt_hw_interrupt_enable(level);

            result = RT_EOK;
        }
        break;

        case USR_DEVICE_USART_CMD_STOP:
        {
            rt_base_t level = rt_hw_interrupt_disable();
            if(usart->init_ok)
                HAL_UART_Abort(usart->config->handle);
            rt_hw_interrupt_enable(level);

            result = RT_EOK;
        }
        break;

        default:
        break;
    }

    return result;
}

static struct usr_device_usart *get_drv_by_handle(UART_HandleTypeDef *huart)
{
    rt_slist_t *node;
    rt_slist_for_each(node, &drv_usart_header)
    {
        struct usr_device_usart *usart = rt_slist_entry(node, struct usr_device_usart, slist);
        if(usart->config->handle == huart)
            return usart;
    }

    return RT_NULL;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    struct usr_device_usart *usart = get_drv_by_handle(huart);
    if(usart == RT_NULL)
        return;

    int result = -RT_ERROR;
    do
    {
        if(usart->tx_activated != RT_TRUE)
            break;
        
        rt_uint8_t *send_ptr = RT_NULL;
        int send_len = rt_ringbuffer_peak(&(usart->tx_rb), &send_ptr);
        if(send_len <= 0)
            break;
        
        HAL_UART_AbortTransmit(usart->config->handle);
        HAL_UART_Transmit_DMA(usart->config->handle, send_ptr, send_len);

        result = RT_EOK;
    }while(0);
    
    if(result == RT_EOK)
    {
        usart->tx_activated = RT_TRUE;
        usart->tx_activated_timeout = rt_tick_get() + rt_tick_from_millisecond(USR_DEVICE_USART_TX_ACTIVATED_TIMEOUT * 1000);
    }
    else
    {
        DRV_USART_RS485_RECV();
        usart->tx_activated = RT_FALSE;
    }

    if(usart->parent.tx_complete)
        usart->parent.tx_complete(&(usart->parent), RT_NULL);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    struct usr_device_usart *usart = get_drv_by_handle(huart);
    if(usart == RT_NULL)
        return;
    
    rt_ringbuffer_putchar(&(usart->rx_rb), usart->ignore_data);
    usart->rx_timeout = rt_tick_get() + rt_tick_from_millisecond(USR_DEVICE_USART_RX_TIMEOUT * 1000);

    HAL_UART_Receive_IT(usart->config->handle, &(usart->ignore_data), 1);
    
    if(usart->parent.rx_indicate)
        usart->parent.rx_indicate(&(usart->parent), 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    struct usr_device_usart *usart = get_drv_by_handle(huart);
    if(usart == RT_NULL)
        return;
    
    usart->reset_flag = 1;
}

void DMA1_Channel4_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&hdma_usart1_tx);

    /* leave interrupt */
    rt_interrupt_leave();
}

void USART1_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_UART_IRQHandler(&huart1);

    /* leave interrupt */
    rt_interrupt_leave();
}

void DMA1_Channel7_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&hdma_usart2_tx);

    /* leave interrupt */
    rt_interrupt_leave();
}

void USART2_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_UART_IRQHandler(&huart2);
    
    /* leave interrupt */
    rt_interrupt_leave();
}

void DMA1_Channel2_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&hdma_usart3_tx);

    /* leave interrupt */
    rt_interrupt_leave();
}

void USART3_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_UART_IRQHandler(&huart3);
    
    /* leave interrupt */
    rt_interrupt_leave();
}

static int drv_hw_usart_init(void)
{
    int obj_num = sizeof(usart_obj) / sizeof(usart_obj[0]);
    struct usr_device_usart_parameter parameter = USR_DEVICE_USART_PARAMETER_DEFAULT;

    for (int i = 0; i < obj_num; i++)
    {
        usart_obj[i].parameter = parameter;
        usart_obj[i].config = &usart_config[i];
        rt_slist_init(&(usart_obj[i].slist));
        rt_slist_append(&drv_usart_header, &(usart_obj[i].slist));
        
        usart_obj[i].parent.init = _usart_init;
        usart_obj[i].parent.read = _usart_read;
        usart_obj[i].parent.write = _usart_write;
        usart_obj[i].parent.control = _usart_control;

        usr_device_register(&(usart_obj[i].parent), usart_obj[i].config->name);
    }

    return RT_EOK;
}
INIT_BOARD_EXPORT(drv_hw_usart_init);


#include "main_hook.h"


static void drv_usart_monitor(void)
{
    static rt_tick_t process_timeout = 2000;

    if((rt_tick_get() - process_timeout) >= (RT_TICK_MAX / 2))
        return;

    rt_slist_t *node;
    rt_slist_for_each(node, &drv_usart_header)
    {
        struct usr_device_usart *usart = rt_slist_entry(node, struct usr_device_usart, slist);
        if(!usart->init_ok)
            continue;
        
        rt_base_t level;

        level = rt_hw_interrupt_disable();
        do
        {
            if(usart->tx_activated != RT_TRUE)
                break;
            
            if ((rt_tick_get() - usart->tx_activated_timeout) < (RT_TICK_MAX / 2))
            {
                usart->error_cnt++;
                HAL_UART_AbortTransmit(usart->config->handle);
                rt_ringbuffer_reset(&(usart->tx_rb));
                usart->tx_activated = RT_FALSE;
                usart->tx_activated_timeout = rt_tick_get();
                DRV_USART_RS485_RECV();
            }
        }while(0);

        if ((rt_tick_get() - usart->rx_timeout) < (RT_TICK_MAX / 2))
            usart->reset_flag = 1;
        rt_hw_interrupt_enable(level);

        if(usart->error_cnt >= USR_DEVICE_USART_MAX_ERROR_CNT)
            usart->reset_flag = 1;

        if(usart->reset_flag)
        {
            level = rt_hw_interrupt_disable();
            do
            {
                if(usart->tx_activated == RT_TRUE)
                    break;
                
                _usart_init(&(usart->parent));
            }while(0);
            rt_hw_interrupt_enable(level);
        }
    }

    process_timeout = rt_tick_get() + rt_tick_from_millisecond(2000);
}

static struct main_hook_module _hook_module;

static int drv_usart_monitor_init(void)
{
    _hook_module.hook = drv_usart_monitor;
    main_hook_module_register(&_hook_module);

    return RT_EOK;
}
INIT_PREV_EXPORT(drv_usart_monitor_init);
