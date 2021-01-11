#ifndef __OLED_H
#define __OLED_H
#include <rtthread.h>
#include "usr_device.h"

#define USR_DEVICE_OLED_CMD_RECT_UPDATE     0x01

struct usr_device_oled
{
    struct usr_device parent;
    struct rt_semaphore sem_lock;
};

#endif
