/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2017-07-24     Tanek        the first version
 * 2018-11-12     Ernest Chen  modify copyright
 */
 
#include <stdint.h>
#include <rthw.h>
#include <rtthread.h>
#include "stm32f1xx_hal.h"

// Updates the variable SystemCoreClock and must be called 
// whenever the core clock is changed during program execution.
extern void SystemCoreClockUpdate(void);

// Holds the system core clock, which is the system clock 
// frequency supplied to the SysTick timer and the processor 
// core clock.
extern uint32_t SystemCoreClock;

/* SysTick configuration */
static void rt_hw_systick_init(void)
{
    HAL_SYSTICK_Config(SystemCoreClock / RT_TICK_PER_SECOND);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

#if defined(RT_USING_USER_MAIN) && defined(RT_USING_HEAP)
#define RT_HEAP_SIZE 1024
static uint32_t rt_heap[RT_HEAP_SIZE];     // heap default size: 4K(1024 * 4)
RT_WEAK void *rt_heap_begin_get(void)
{
    return rt_heap;
}

RT_WEAK void *rt_heap_end_get(void)
{
    return rt_heap + RT_HEAP_SIZE;
}
#endif

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    return HAL_OK;
}

extern void SystemClock_Config(void);
extern int console_init(void);

/**
 * This function will initial your board.
 */
void rt_hw_board_init()
{
    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    /* System Clock Update */
    SystemCoreClockUpdate();
    
    /* System Tick Configuration */
    rt_hw_systick_init();

    /* Call components board initial (use INIT_BOARD_EXPORT()) */
#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif

#if defined(RT_USING_USER_MAIN) && defined(RT_USING_HEAP)
    rt_system_heap_init(rt_heap_begin_get(), rt_heap_end_get());
#endif
    
    console_init();
}

void SysTick_Handler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_IncTick();
    rt_tick_increase();

    /* leave interrupt */
    rt_interrupt_leave();
}
