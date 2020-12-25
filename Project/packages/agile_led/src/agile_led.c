#include <agile_led.h>
#include <stdlib.h>
#include <string.h>

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME    "agile_led"
#ifdef PKG_AGILE_LED_DEBUG
#define DBG_LEVEL           DBG_LOG
#else
#define DBG_LEVEL           DBG_INFO
#endif
#include <rtdbg.h>

// agile_led 线程堆栈大小
#ifndef PKG_AGILE_LED_THREAD_STACK_SIZE
#define PKG_AGILE_LED_THREAD_STACK_SIZE 256
#endif

// agile_led 线程优先级
#ifndef PKG_AGILE_LED_THREAD_PRIORITY
#define PKG_AGILE_LED_THREAD_PRIORITY RT_THREAD_PRIORITY_MAX - 2
#endif

ALIGN(RT_ALIGN_SIZE)
// agile_led 线程
static rt_uint8_t agile_led_stack[PKG_AGILE_LED_THREAD_STACK_SIZE];
static struct rt_thread agile_led_thread;
// agile_led 单向链表
static rt_slist_t agile_led_list = RT_SLIST_OBJECT_INIT(agile_led_list);
// agile_led 互斥锁
static struct rt_mutex lock_mtx;
// agile_led 初始化完成标志
static uint8_t is_initialized = 0;


static void agile_led_default_compelete_callback(agile_led_t *led)
{
    RT_ASSERT(led);
    LOG_D("led pin:%d compeleted.", led->pin);
}

int agile_led_init(agile_led_t *led, rt_base_t pin, rt_base_t active_logic, const uint32_t *light_array, int array_size, int32_t loop_cnt)
{
    if (!is_initialized)
    {
        LOG_E("Agile led haven't initialized!");
        return -RT_ERROR;
    }
    
    rt_memset(led, 0, sizeof(agile_led_t));
    led->active = 0;
    led->pin = pin;
    led->active_logic = active_logic;
    led->light_arr = light_array;
    led->arr_num = array_size;
    led->arr_index = 0;
    led->loop_init = loop_cnt;
    led->loop_cnt = led->loop_init;
    led->tick_timeout = rt_tick_get();
    led->compelete = agile_led_default_compelete_callback;
    rt_slist_init(&(led->slist));

    drv_pin_mode(pin, PIN_MODE_OUTPUT);
    drv_pin_write(pin, !active_logic);

    return RT_EOK;
}

int agile_led_start(agile_led_t *led)
{ 
    RT_ASSERT(led);
    rt_mutex_take(&lock_mtx, RT_WAITING_FOREVER);
    if(led->active)
    {
        rt_mutex_release(&lock_mtx);
        return -RT_ERROR;
    }
    if((led->light_arr == RT_NULL) || (led->arr_num == 0))
    {
        rt_mutex_release(&lock_mtx);
        return -RT_ERROR;
    }
    led->arr_index = 0;
    led->loop_cnt = led->loop_init;
    led->tick_timeout = rt_tick_get();
    rt_slist_append(&(agile_led_list), &(led->slist));
    led->active = 1;
    rt_mutex_release(&lock_mtx);
    return RT_EOK;
}

int agile_led_stop(agile_led_t *led)
{
    RT_ASSERT(led);
    rt_mutex_take(&lock_mtx, RT_WAITING_FOREVER);
    if(!led->active)
    {
        rt_mutex_release(&lock_mtx);
        return RT_EOK;
    }
    rt_slist_remove(&(agile_led_list), &(led->slist));
    led->slist.next = RT_NULL;
    led->active = 0;
    rt_mutex_release(&lock_mtx);
    return RT_EOK;
}

int agile_led_set_light_mode(agile_led_t *led, const uint32_t *light_array, int array_size, int32_t loop_cnt)
{
    RT_ASSERT(led);
    rt_mutex_take(&lock_mtx, RT_WAITING_FOREVER);
    led->light_arr = light_array;
    led->arr_num = array_size;
    led->arr_index = 0;
    led->loop_init = loop_cnt;
    led->loop_cnt = led->loop_init;
    led->tick_timeout = rt_tick_get();
    rt_mutex_release(&lock_mtx);
    return RT_EOK;
}

int agile_led_set_compelete_callback(agile_led_t *led, void (*compelete)(agile_led_t *led))
{
    RT_ASSERT(led);
    rt_mutex_take(&lock_mtx, RT_WAITING_FOREVER);
    led->compelete = compelete;
    rt_mutex_release(&lock_mtx);
    return RT_EOK;
}

void agile_led_toggle(agile_led_t *led)
{
    RT_ASSERT(led);
    drv_pin_write(led->pin, !drv_pin_read(led->pin));
}

void agile_led_on(agile_led_t *led)
{
    RT_ASSERT(led);
    drv_pin_write(led->pin, led->active_logic);
}

void agile_led_off(agile_led_t *led)
{
    RT_ASSERT(led);
    drv_pin_write(led->pin, !led->active_logic);
}

static void led_process(void *parameter)
{
    rt_slist_t *node;
    while (1)
    {
        rt_mutex_take(&lock_mtx, RT_WAITING_FOREVER);
        rt_slist_for_each(node, &(agile_led_list))
        {
            agile_led_t *led = rt_slist_entry(node, agile_led_t, slist);
            if(led->loop_cnt == 0)
            {
                agile_led_stop(led);
                if(led->compelete)
                {
                    led->compelete(led);
                }
                node = &agile_led_list;
                continue;
            }
        __repeat:
            if((rt_tick_get() - led->tick_timeout) < (RT_TICK_MAX / 2))
            {
                if(led->arr_index < led->arr_num)
                {
                    if (led->light_arr[led->arr_index] == 0)
                    {
                        led->arr_index++;
                        goto __repeat;
                    }
                    if(led->arr_index % 2)
                    {
                        agile_led_off(led);
                    }
                    else
                    {
                        agile_led_on(led);
                    }
                    led->tick_timeout = rt_tick_get() + rt_tick_from_millisecond(led->light_arr[led->arr_index]);
                    led->arr_index++;
                }
                else
                {
                    led->arr_index = 0;
                    if(led->loop_cnt > 0)
                        led->loop_cnt--;
                }
            }
        }
        rt_mutex_release(&lock_mtx);
        rt_thread_mdelay(5);
    }
}


static int agile_led_env_init(void)
{
    rt_mutex_init(&lock_mtx, "led_mtx", RT_IPC_FLAG_FIFO);

    rt_thread_init(&agile_led_thread,
                   "agile_led",
                   led_process,
                   RT_NULL,
                   &agile_led_stack[0],
                   sizeof(agile_led_stack),
                   PKG_AGILE_LED_THREAD_PRIORITY,
                   100);
    rt_thread_startup(&agile_led_thread);

    is_initialized = 1;
    return RT_EOK;
}
INIT_APP_EXPORT(agile_led_env_init);
