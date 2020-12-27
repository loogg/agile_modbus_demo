#ifndef __LED_H
#define __LED_H
#include <rtthread.h>

enum led_cmd_t
{
    LED_CMD_NULL = 0,
    LED_CMD_WIFI_RESET,
    LED_CMD_WIFI_START_SMART,
    LED_CMD_WIFI_CONNECTED,
    LED_CMD_WIFI_DISCONNECT,
    LED_CMD_WIFI_DATA
};

int led_control(enum led_cmd_t cmd);

#endif
