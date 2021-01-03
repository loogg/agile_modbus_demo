#include <agile_button.h>
#include "init_module.h"
#include "wifi.h"

#define KEY_PIN                 2

ALIGN(RT_ALIGN_SIZE)
static struct agile_btn key;
static struct wifi_device *wifi_dev = RT_NULL;

static void btn_click_event_cb(agile_btn_t *btn)
{
    if((btn->hold_time >= 100) && (btn->hold_time <= 1000))
    {
        usr_device_control(&(wifi_dev->parent), USR_DEVICE_WIFI_CMD_SMART, (void *)(!wifi_dev->smart_flag));
    }
}

static int key_init(void)
{
    wifi_dev = (struct wifi_device *)usr_device_find("wifi");
    RT_ASSERT(wifi_dev);

    agile_btn_init(&key, KEY_PIN, PIN_HIGH, PIN_MODE_INPUT_PULLDOWN);
    RT_ASSERT(agile_btn_set_event_cb(&key, BTN_CLICK_EVENT, btn_click_event_cb) == RT_EOK);
    RT_ASSERT(agile_btn_start(&key) == RT_EOK);

    return RT_EOK;
}

static struct init_module key_init_module;

static int key_init_module_register(void)
{
    key_init_module.init = key_init;
    init_module_app_register(&key_init_module);

    return RT_EOK;
}
INIT_DEVICE_EXPORT(key_init_module_register);
