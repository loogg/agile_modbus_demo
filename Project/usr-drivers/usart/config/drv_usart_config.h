#ifndef __DRV_USART_CONFIG_H
#define __DRV_USART_CONFIG_H

/* usart1 config */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart1_rx;
#define USART1_CONFIG               \
    {                               \
        .name = "usart1",           \
        .handle = &huart1,          \
        .instance = USART1,         \
        .dma_tx = &hdma_usart1_tx,  \
        .dma_rx = &hdma_usart1_rx,  \
        .rs485_control_pin = -1,    \
        .rs485_send_logic = -1      \
    }

/* usart2 config */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
#define USART2_CONFIG               \
    {                               \
        .name = "usart2",           \
        .handle = &huart2,          \
        .instance = USART2,         \
        .dma_tx = &hdma_usart2_tx,  \
        .dma_rx = &hdma_usart2_rx,  \
        .rs485_control_pin = -1,    \
        .rs485_send_logic = -1      \
    }

/* usart3 config */
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef hdma_usart3_tx;
extern DMA_HandleTypeDef hdma_usart3_rx;
#define USART3_CONFIG               \
    {                               \
        .name = "usart3",           \
        .handle = &huart3,          \
        .instance = USART3,         \
        .dma_tx = &hdma_usart3_tx,  \
        .dma_rx = &hdma_usart3_rx,  \
        .rs485_control_pin = -1,    \
        .rs485_send_logic = -1      \
    }

#endif
