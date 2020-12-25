#include "stm32f1xx_hal.h"
#include <rtthread.h>

static IWDG_HandleTypeDef hiwdg;

static void feed_wdt(void)
{
#define FEED_WDT_CYCLE  1

    static rt_tick_t feed_timeout = 0;

    if((rt_tick_get() - feed_timeout) < (RT_TICK_MAX / 2))
    {
        HAL_IWDG_Refresh(&hiwdg);
        feed_timeout = rt_tick_get() + rt_tick_from_millisecond(FEED_WDT_CYCLE * 1000);
    }
}

static int drv_wdt_init(void)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_128;
    hiwdg.Init.Reload = 4000; // 4000 * 3.2ms

    if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
    {
        rt_thread_mdelay(100);
        HAL_NVIC_SystemReset();
    }

    rt_thread_idle_sethook(feed_wdt);
    
    return RT_EOK;
}
INIT_PREV_EXPORT(drv_wdt_init);
