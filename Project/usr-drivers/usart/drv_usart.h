#ifndef __DRV_USART_H
#define __DRV_USART_H
#include <rtthread.h>
#include "ringbuffer.h"
#include "stm32f1xx_hal.h"
#include "usr_device.h"

#define USR_DEVICE_USART_TX_ACTIVATED_TIMEOUT   15

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

#define USR_DEVICE_USART_ERROR_TX_TIMEOUT       0x01
#define USR_DEVICE_USART_ERROR_TX_RB_SAVE       0x02
#define USR_DEVICE_USART_ERROR_RX_RB_FULL       0x04
#define USR_DEVICE_USART_ERROR_OTHER            0x08

struct usr_device_usart_config
{
    const char *name;
    UART_HandleTypeDef *handle;
    USART_TypeDef *instance;
    DMA_HandleTypeDef *dma_tx;
    DMA_HandleTypeDef *dma_rx;
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
    struct usr_device_usart_buffer buffer;
    struct rt_ringbuffer tx_rb;
    rt_uint16_t need_send;
    struct rt_ringbuffer rx_rb;
    rt_uint16_t rx_index;
    rt_uint8_t tx_activated;
    rt_tick_t tx_activated_timeout;
    struct usr_device_usart_parameter parameter;
    const struct usr_device_usart_config *config;
    rt_slist_t slist;
};

#endif
