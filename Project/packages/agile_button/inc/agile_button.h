#ifndef __PKG_AGILE_BUTTON_H
#define __PKG_AGILE_BUTTON_H
#include <rtthread.h>
#include "drv_gpio.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum agile_btn_event
{
    BTN_PRESS_DOWN_EVENT = 0,
    BTN_HOLD_EVENT,
    BTN_PRESS_UP_EVENT,
    BTN_CLICK_EVENT,
    BTN_EVENT_SUM
};

enum agile_btn_state
{
    BTN_STATE_NONE_PRESS = 0,
    BTN_STATE_CHECK_PRESS,
    BTN_STATE_PRESS_DOWN,
    BTN_STATE_PRESS_HOLD,
    BTN_STATE_PRESS_UP,
};

// agile_button 结构体
typedef struct agile_btn agile_btn_t;

struct agile_btn
{
    uint8_t active;                                            // 激活标志
    uint8_t repeat_cnt;                                        // 按键重按计数
    uint8_t elimination_time;                                  // 按键消抖时间(单位ms,默认15ms)
    enum agile_btn_event event;                                // 按键对象事件
    enum agile_btn_state state;                                // 按键对象状态
    uint32_t hold_time;                                        // 按键按下持续时间(单位ms)
    uint32_t prev_hold_time;                                   // 缓存hold_time变量
    uint32_t hold_cycle_time;                                  // 按键按下后持续调用回调函数的周期(单位ms,默认1s)
    rt_base_t pin;                                             // 按键引脚
    rt_base_t active_logic;                                    // 有效电平(PIN_HIGH/PIN_LOW)
    rt_tick_t tick_timeout;                                    // 超时时间
    void (*event_cb[BTN_EVENT_SUM])(agile_btn_t *btn);         // 按键对象事件回调函数
    void (*hook)(agile_btn_t *btn);                            // 按键钩子回调函数
    rt_slist_t slist;                                          // 单向链表节点
};

// 初始化按键对象
int agile_btn_init(agile_btn_t *btn, rt_base_t pin, rt_base_t active_logic, rt_base_t pin_mode);
// 启动按键
int agile_btn_start(agile_btn_t *btn);
// 停止按键
int agile_btn_stop(agile_btn_t *btn);
// 设置按键消抖时间
int agile_btn_set_elimination_time(agile_btn_t *btn, uint8_t elimination_time);
// 设置按键按下后BTN_HOLD_EVENT事件回调函数的周期
int agile_btn_set_hold_cycle_time(agile_btn_t *btn, uint32_t hold_cycle_time);
// 设置按键事件回调函数
int agile_btn_set_event_cb(agile_btn_t *btn, enum agile_btn_event event, void (*event_cb)(agile_btn_t *btn));
// 设置按键钩子回调函数
int agile_btn_set_hook(agile_btn_t *btn, void (*hook)(agile_btn_t *btn));

#ifdef __cplusplus
}
#endif

#endif
