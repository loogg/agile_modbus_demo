#ifndef __PKG_AGILE_LED_H
#define __PKG_AGILE_LED_H
#include <rtthread.h>
#include "drv_gpio.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// agile_led 结构体
typedef struct agile_led agile_led_t;

struct agile_led
{
    uint8_t active;                                  // 激活标志
    rt_base_t pin;                                   // 控制引脚
    rt_base_t active_logic;                          // 有效电平(PIN_HIGH/PIN_LOW)
    const uint32_t *light_arr;                             // 闪烁数组
    uint32_t arr_num;                                // 数组元素数目
    uint32_t arr_index;                              // 数组索引
    int32_t loop_init;                               // 循环次数
    int32_t loop_cnt;                                // 循环次数计数
    rt_tick_t tick_timeout;                          // 超时时间
    void (*compelete)(agile_led_t *led);             // 操作完成回调函数
    rt_slist_t slist;                                // 单向链表节点
};

// 初始化led对象
int agile_led_init(agile_led_t *led, rt_base_t pin, rt_base_t active_logic, const uint32_t *light_array, int array_size, int32_t loop_cnt);
// 启动led对象,根据设置的模式执行动作
int agile_led_start(agile_led_t *led);
// 停止led对象
int agile_led_stop(agile_led_t *led);
// 设置led对象的模式
int agile_led_set_light_mode(agile_led_t *led, const uint32_t *light_array, int array_size, int32_t loop_cnt);
// 设置led对象操作完成的回调函数
int agile_led_set_compelete_callback(agile_led_t *led, void (*compelete)(agile_led_t *led));
// led对象电平翻转
void agile_led_toggle(agile_led_t *led);
// led对象亮
void agile_led_on(agile_led_t *led);
// led对象灭
void agile_led_off(agile_led_t *led);

#ifdef __cplusplus
}
#endif

#endif
