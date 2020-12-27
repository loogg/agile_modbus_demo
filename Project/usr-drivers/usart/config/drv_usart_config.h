#ifndef __DRV_USART_CONFIG_H
#define __DRV_USART_CONFIG_H

/* usart1 config */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_tx;
#define USART1_CONFIG               \
    {                               \
        .name = "usart1",           \
        .handle = &huart1,          \
        .instance = USART1,         \
        .dma_tx = &hdma_usart1_tx,  \
        .rs485_control_pin = -1,    \
        .rs485_send_logic = -1      \
    }

/* usart2 config */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_tx;
#define USART2_CONFIG               \
    {                               \
        .name = "usart2",           \
        .handle = &huart2,          \
        .instance = USART2,         \
        .dma_tx = &hdma_usart2_tx,  \
        .rs485_control_pin = -1,    \
        .rs485_send_logic = -1      \
    }

/* usart3 config */
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef hdma_usart3_tx;
#define USART3_CONFIG               \
    {                               \
        .name = "usart3",           \
        .handle = &huart3,          \
        .instance = USART3,         \
        .dma_tx = &hdma_usart3_tx,  \
        .rs485_control_pin = -1,    \
        .rs485_send_logic = -1      \
    }

#endif
