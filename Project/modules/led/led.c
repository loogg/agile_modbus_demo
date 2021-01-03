#include "led.h"
#include <agile_led.h>
#include "init_module.h"

#define D0_LED_PIN      0
#define D1_LED_PIN      1

#define ITEM_NUM(items) sizeof(items) / sizeof(items[0])

ALIGN(RT_ALIGN_SIZE)
static struct agile_led d0_led;
static struct agile_led d1_led;
static struct rt_mailbox mb;
static rt_uint8_t mb_pool[400];
static rt_uint8_t led_stack[256];
static struct rt_thread led_thread;
static const uint32_t light_mode_50ms[] = {50, 50};
static const uint32_t light_mode_100ms[] = {100, 100};
static const uint32_t light_mode_1000ms[] = {1000, 1000};

static void led_msg(void *parameter)
{
    rt_ubase_t cmd = LED_CMD_NULL;
    rt_uint8_t wifi_connected = 0;

    while(1)
    {
        rt_err_t rc = rt_mb_recv(&mb, &cmd, RT_WAITING_FOREVER);
        if(rc != RT_EOK)
            continue;
        
        switch(cmd)
        {
            case LED_CMD_WIFI_RESET:
            {
                RT_ASSERT(agile_led_stop(&d0_led) == RT_EOK);
                agile_led_off(&d0_led);
                wifi_connected = 0;
            }
            break;

            case LED_CMD_WIFI_START_SMART:
            {
                RT_ASSERT(agile_led_stop(&d0_led) == RT_EOK);
                agile_led_set_light_mode(&d0_led, light_mode_100ms, ITEM_NUM(light_mode_100ms), -1);
                RT_ASSERT(agile_led_start(&d0_led) == RT_EOK);
                wifi_connected = 0;
            }
            break;

            case LED_CMD_WIFI_CONNECTED:
            {
                RT_ASSERT(agile_led_stop(&d0_led) == RT_EOK);
                agile_led_on(&d0_led);
                wifi_connected = 1;
            }
            break;

            case LED_CMD_WIFI_DISCONNECT:
            {
                RT_ASSERT(agile_led_stop(&d0_led) == RT_EOK);
                agile_led_set_light_mode(&d0_led, light_mode_1000ms, ITEM_NUM(light_mode_1000ms), -1);
                RT_ASSERT(agile_led_start(&d0_led) == RT_EOK);
                wifi_connected = 0;
            }
            break;

            case LED_CMD_WIFI_DATA:
            {
                if(wifi_connected)
                {
                    RT_ASSERT(agile_led_stop(&d1_led) == RT_EOK);
                    RT_ASSERT(agile_led_start(&d1_led) == RT_EOK);
                }
            }
            break;

            default:
            break;
        }
    }
}

int led_control(enum led_cmd_t cmd)
{
    rt_ubase_t value = cmd;
    rt_mb_send(&mb, value);
    return RT_EOK;
}

static int led_init(void)
{
    agile_led_init(&d0_led, D0_LED_PIN, PIN_LOW, light_mode_1000ms, ITEM_NUM(light_mode_1000ms), -1);
    agile_led_init(&d1_led, D1_LED_PIN, PIN_LOW, light_mode_50ms, ITEM_NUM(light_mode_50ms), 2);

    rt_thread_mdelay(100);
    agile_led_on(&d0_led);
    agile_led_on(&d1_led);
    rt_thread_mdelay(200);
    agile_led_off(&d0_led);
    agile_led_off(&d1_led);

    rt_mb_init(&mb, "led", &mb_pool[0], sizeof(mb_pool) / 4, RT_IPC_FLAG_FIFO);

    rt_thread_init(&led_thread,
                   "led_msg",
                   led_msg,
                   RT_NULL,
                   &led_stack[0],
                   sizeof(led_stack),
                   0,
                   100);
    rt_thread_startup(&led_thread);

    return RT_EOK;
}

static struct init_module led_init_module;

static int led_init_module_register(void)
{
    led_init_module.init = led_init;
    init_module_prev_register(&led_init_module);

    return RT_EOK;
}
INIT_PREV_EXPORT(led_init_module_register);
