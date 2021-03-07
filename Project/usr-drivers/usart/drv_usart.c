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
    
    dev->error = 0;
    rt_ringbuffer_init(&(usart->tx_rb), usart->buffer.send_buf, usart->buffer.send_bufsz);
    usart->need_send = 0;
    rt_ringbuffer_init(&(usart->rx_rb), usart->buffer.read_buf, usart->buffer.read_bufsz);
    usart->rx_index = usart->rx_rb.buffer_size;
    usart->tx_activated = RT_FALSE;
    usart->tx_activated_timeout = rt_tick_get();
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
    HAL_UART_Receive_DMA(usart->config->handle, usart->rx_rb.buffer_ptr, usart->rx_rb.buffer_size);

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
    if(size == 0)
        return 0;
    if(dev->error)
        return 0;
    
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
    if(size == 0)
        return 0;
    if(dev->error)
        return 0;

    rt_base_t level = rt_hw_interrupt_disable();
    if (usart->tx_activated == RT_TRUE)
    {
        if ((rt_tick_get() - usart->tx_activated_timeout) < (RT_TICK_MAX / 2))
        {
            dev->error |= USR_DEVICE_USART_ERROR_TX_TIMEOUT;
            rt_hw_interrupt_enable(level);
            return 0;
        }
    }
    rt_uint16_t write_index = usart->tx_rb.write_index;
    rt_uint8_t tx_activated = usart->tx_activated;
    rt_size_t put_len = rt_ringbuffer_put_update(&(usart->tx_rb), size);
    if ((put_len > 0) && (usart->tx_activated != RT_TRUE))
    {
        usart->tx_activated = RT_TRUE;
        usart->tx_activated_timeout = rt_tick_get() + rt_tick_from_millisecond(USR_DEVICE_USART_TX_ACTIVATED_TIMEOUT * 1000);
    }
    rt_hw_interrupt_enable(level);

    if(put_len == 0)
        return 0;
    
    rt_ringbuffer_put_raw(&(usart->tx_rb), write_index, buffer, put_len);
    
    level = rt_hw_interrupt_disable();
    usart->need_send += put_len;
    if(tx_activated == RT_TRUE)
    {
        rt_hw_interrupt_enable(level);
        return put_len;
    }
    rt_uint8_t *send_ptr = RT_NULL;
    rt_size_t send_len = rt_ringbuffer_peak(&(usart->tx_rb), &send_ptr, usart->need_send);
    if(send_len == 0)
    {
        dev->error |= USR_DEVICE_USART_ERROR_TX_RB_SAVE;
        rt_hw_interrupt_enable(level);
        return 0;
    }
    usart->need_send -= send_len;
    DRV_USART_RS485_SEND();
    HAL_UART_AbortTransmit(usart->config->handle);
    HAL_UART_Transmit_DMA(usart->config->handle, send_ptr, send_len);
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
            usart->parameter = *parameter;
            if(usart->init_ok)
                _usart_init(dev);
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
            usart->buffer = *buffer;
            if(usart->init_ok)
                _usart_init(dev);
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
            usart->need_send = 0;
            usart->rx_index = usart->rx_rb.buffer_size;
            usart->tx_activated = RT_FALSE;
            usart->tx_activated_timeout = rt_tick_get();
            DRV_USART_RS485_RECV();
            HAL_UART_Receive_DMA(usart->config->handle, usart->rx_rb.buffer_ptr, usart->rx_rb.buffer_size);
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

    usr_device_t dev = &(usart->parent);
    int result = -RT_ERROR;
    do
    {
        if(usart->tx_activated != RT_TRUE)
            break;
        if(usart->need_send == 0)
            break;
        if(dev->error)
            break;
        
        rt_uint8_t *send_ptr = RT_NULL;
        rt_size_t send_len = rt_ringbuffer_peak(&(usart->tx_rb), &send_ptr, usart->need_send);
        if(send_len == 0)
        {
            dev->error |= USR_DEVICE_USART_ERROR_TX_RB_SAVE;
            break;
        }
        usart->need_send -= send_len;
        HAL_UART_AbortTransmit(usart->config->handle);
        HAL_UART_Transmit_DMA(usart->config->handle, send_ptr, send_len);

        result = RT_EOK;
    }while(0);
    
    if((usart->tx_activated == RT_TRUE) && usart->parent.tx_complete)
        usart->parent.tx_complete(&(usart->parent), RT_NULL);

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
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    struct usr_device_usart *usart = get_drv_by_handle(huart);
    if(usart == RT_NULL)
        return;
    
    usr_device_t dev = &(usart->parent);
    if(dev->error)
        return;
    
    uint32_t index = __HAL_DMA_GET_COUNTER(usart->config->dma_rx);
    uint16_t recv_len = usart->rx_index + usart->rx_rb.buffer_size - index;
    usart->rx_index = index;

    if(recv_len > 0)
    {
        if(rt_ringbuffer_put_update(&(usart->rx_rb), recv_len) != recv_len)
            dev->error |= USR_DEVICE_USART_ERROR_RX_RB_FULL;

        if(usart->parent.rx_indicate)
            usart->parent.rx_indicate(&(usart->parent), recv_len);
    }
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    struct usr_device_usart *usart = get_drv_by_handle(huart);
    if(usart == RT_NULL)
        return;
    
    usr_device_t dev = &(usart->parent);
    if(dev->error)
        return;
    
    uint32_t index = __HAL_DMA_GET_COUNTER(usart->config->dma_rx);
    uint16_t recv_len = usart->rx_index - index;
    usart->rx_index = index;
    if(recv_len > 0)
    {
        if(rt_ringbuffer_put_update(&(usart->rx_rb), recv_len) != recv_len)
            dev->error |= USR_DEVICE_USART_ERROR_RX_RB_FULL;
        
        if(usart->parent.rx_indicate)
            usart->parent.rx_indicate(&(usart->parent), recv_len);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    struct usr_device_usart *usart = get_drv_by_handle(huart);
    if(usart == RT_NULL)
        return;
    
    usr_device_t dev = &(usart->parent);
    dev->error |= USR_DEVICE_USART_ERROR_OTHER;
}

void DMA1_Channel4_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&hdma_usart1_tx);

    /* leave interrupt */
    rt_interrupt_leave();
}

void DMA1_Channel5_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&hdma_usart1_rx);

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

void DMA1_Channel6_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&hdma_usart2_rx);

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

void DMA1_Channel3_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&hdma_usart3_rx);

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

static void idle_hook_cb(void)
{
    rt_slist_t *node;
    rt_slist_for_each(node, &drv_usart_header)
    {
        struct usr_device_usart *usart = rt_slist_entry(node, struct usr_device_usart, slist);
        if(!usart->init_ok)
            continue;

        usr_device_t dev = &(usart->parent);
        uint32_t index = __HAL_DMA_GET_COUNTER(usart->config->dma_rx);
        if((index != usart->rx_index) && (index < usart->rx_index))
        {
            rt_base_t level = rt_hw_interrupt_disable();
            do
            {
                index = __HAL_DMA_GET_COUNTER(usart->config->dma_rx);
                if(index >= usart->rx_index)
                    break;
                if(__HAL_DMA_GET_FLAG(usart->config->dma_rx, __HAL_DMA_GET_HT_FLAG_INDEX(usart->config->dma_rx)) != RESET)
                    break;
                if(__HAL_DMA_GET_FLAG(usart->config->dma_rx, __HAL_DMA_GET_TC_FLAG_INDEX(usart->config->dma_rx)) != RESET)
                    break;

                uint16_t recv_len = usart->rx_index - index;
                usart->rx_index = index;
                if(rt_ringbuffer_put_update(&(usart->rx_rb), recv_len) != recv_len)
                    dev->error |= USR_DEVICE_USART_ERROR_RX_RB_FULL;

                if(usart->parent.rx_indicate)
                    usart->parent.rx_indicate(&(usart->parent), recv_len);
            }while(0);
            rt_hw_interrupt_enable(level);
        }
    }
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

    rt_thread_idle_sethook(idle_hook_cb);

    return RT_EOK;
}
INIT_BOARD_EXPORT(drv_hw_usart_init);
