#include <rtthread.h>
#include "main_hook.h"
#include <stdlib.h>

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME    "runtime"
#define DBG_LEVEL           DBG_LOG
#include <rtdbg.h>

static rt_uint32_t run_time = 0;

static void calc_runtime(void)
{
    static rt_tick_t prev_tick = 0;
    static rt_tick_t redress = 0;

    rt_tick_t now_tick = rt_tick_get();
    rt_tick_t diff_tick = 0;

    if(now_tick >= prev_tick)
        diff_tick = now_tick - prev_tick;
    else
        diff_tick = RT_TICK_MAX - prev_tick + now_tick;
    diff_tick += redress;

    rt_uint32_t sec_part = diff_tick / RT_TICK_PER_SECOND;
    if(sec_part > 0)
    {
        prev_tick = now_tick;
        redress = diff_tick % RT_TICK_PER_SECOND;
        run_time += sec_part;
    }
}

static int get_runtime(void)
{
    LOG_I("running [%u]/s", run_time);

    return RT_EOK;
}
MSH_CMD_EXPORT(get_runtime, get runtime);

static struct main_hook_module _hook_module;

static int runtime_init(void)
{
    _hook_module.hook = calc_runtime;
    main_hook_module_register(&_hook_module);

    return RT_EOK;
}
INIT_PREV_EXPORT(runtime_init);
