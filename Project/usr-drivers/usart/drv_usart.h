#ifndef __DRV_USART_H
#define __DRV_USART_H
#include <rtthread.h>
#include "ringbuffer.h"
#include "stm32f1xx_hal.h"
#include "usr_device.h"

#define USR_DEVICE_USART_RX_TIMEOUT             60
#define USR_DEVICE_USART_TX_ACTIVATED_TIMEOUT   10
#define USR_DEVICE_USART_MAX_ERROR_CNT          3

#define USR_DEVICE_USART_PARAMETER_DEFAULT  \
    {                                       \
        9600,                               \
        UART_PARITY_NONE,                   \
        UART_WORDLENGTH_8B,                 \
        UART_STOPBITS_1                     \
    }

#define USR_DEVICE_USART_CMD_SET_PARAMETER      0x01
#define USR_DEVICE_USART_CMD_SET_BUFFER         0x02
#define USR_DEVICE_USART_CMD_FLUSH              0X03
#define USR_DEVICE_USART_CMD_STOP               0x04

struct usr_device_usart_config
{
    const char *name;
    UART_HandleTypeDef *handle;
    USART_TypeDef *instance;
    DMA_HandleTypeDef *dma_tx;
    int rs485_control_pin;
    int rs485_send_logic;
};

struct usr_device_usart_parameter
{
    rt_uint32_t baudrate;
    rt_uint32_t parity;
    rt_uint32_t wlen;
    rt_uint32_t stblen;
};

struct usr_device_usart_buffer
{
    rt_uint8_t *send_buf;
    int send_bufsz;
    rt_uint8_t *read_buf;
    int read_bufsz;
};

struct usr_device_usart
{
    struct usr_device parent;
    rt_uint8_t init_ok;
    rt_uint8_t error_cnt;
    rt_uint8_t reset_flag;
    rt_uint8_t ignore_data;
    struct usr_device_usart_buffer buffer;
    struct rt_ringbuffer tx_rb;
    struct rt_ringbuffer rx_rb;
    rt_uint8_t tx_activated;
    rt_tick_t tx_activated_timeout;
    rt_tick_t rx_timeout;
    struct usr_device_usart_parameter parameter;
    const struct usr_device_usart_config *config;
    rt_slist_t slist;
};

#endif
