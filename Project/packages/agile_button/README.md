# agile_button

## 1、介绍

agile_button是基于RT-Thread实现的button软件包，提供button操作的API。

### 1.1 特性

1. 代码简洁易懂，充分使用RT-Thread提供的API
2. 详细注释
3. 线程安全
4. 断言保护
5. API操作简单

### 1.2 目录结构

| 名称 | 说明 |
| ---- | ---- |
| examples | 例子目录 |
| inc  | 头文件目录 |
| src  | 源代码目录 |

### 1.3 许可证

agile_button package 遵循 LGPLv2.1 许可，详见 `LICENSE` 文件。

### 1.4 依赖

- RT-Thread 3.0+
- RT-Thread 4.0+

## 2、如何打开 agile_button

使用 agile_button package 需要在 RT-Thread 的包管理器中选择它，具体路径如下：

```
RT-Thread online packages
    peripheral libraries and drivers --->
        [*] agile_button: A agile button package
```

然后让 RT-Thread 的包管理器自动更新，或者使用 `pkgs --update` 命令更新包到 BSP 中。

## 3、使用 agile_button

在打开 agile_button package 后，当进行 bsp 编译时，它会被加入到 bsp 工程中进行编译。

### 3.1、API说明

1. 创建按键对象

```C
agile_btn_t *agile_btn_create(rt_base_t pin, rt_base_t active_logic, rt_base_t pin_mode);
```

|参数|注释|
|----|----|
|pin|按键引脚|
|active_logic|有效电平(PIN_HIGH/PIN_LOW)|
|pin_mode|引脚模式|

|返回|注释|
|----|----|
|!=RT_NULL|agile_btn对象指针|
|RT_NULL|异常|

2. 删除按键对象

```C
int agile_btn_delete(agile_btn_t *btn);
```

|参数|注释|
|----|----|
|btn|按键对象|

|返回|注释|
|----|----|
|RT_EOK|成功|

3. 启动按键

```C
int agile_btn_start(agile_btn_t *btn);
```

|参数|注释|
|----|----|
|btn|按键对象|

|返回|注释|
|----|----|
|RT_EOK|成功|
|!=RT_OK|异常|

4. 停止按键

```C
int agile_btn_stop(agile_btn_t *btn);
```

|参数|注释|
|----|----|
|btn|按键对象|

|返回|注释|
|----|----|
|RT_EOK|成功|

5. 设置按键消抖时间

```C
int agile_btn_set_elimination_time(agile_btn_t *btn, uint8_t elimination_time);
```

|参数|注释|
|----|----|
|btn|按键对象|
|elimination_time|消抖时间(单位ms)|

|返回|注释|
|----|----|
|RT_EOK|成功|

6. 设置按键按下后BTN_HOLD_EVENT事件回调函数的周期

```C
int agile_btn_set_hold_cycle_time(agile_btn_t *btn, uint32_t hold_cycle_time);
```

|参数|注释|
|----|----|
|btn|按键对象|
|hold_cycle_time|周期时间(单位ms)|

|返回|注释|
|----|----|
|RT_EOK|成功|

7. 设置按键事件回调函数

```C
int agile_btn_set_event_cb(agile_btn_t *btn, enum agile_btn_event event, void (*event_cb)(agile_btn_t *btn));
```

|参数|注释|
|----|----|
|btn|按键对象|
|event|事件|
|event_cb|回调函数|

|返回|注释|
|----|----|
|RT_EOK|成功|
|!=RT_OK|异常|

### 3.2、示例

使用示例在 [examples](./examples) 下。

## 4、注意事项

1. 调用 `agile_btn_create` API创建完按键对象后，调用其他API确保按键对象创建成功，否则被断言。
2. 两次按下间隔在500ms内记为连续按下， `repeact_cnt` 变量自加，否则为0。应用层可以根据该变量得到连续按下多少次，执行相应的动作。
3. 消抖时间设置可以使用 `agile_btn_set_elimination_time` 该API设置，默认为15ms;BTN_HOLD_EVENT事件回调函数的周期可以使用 `agile_btn_set_hold_cycle_time` 该API设置，默认为1s。
4. 通过 `hold_time` 变量可以知道按下持续时间，应用层可以根据其做相应的操作

## 5、联系方式 & 感谢

* 维护：马龙伟
* 主页：<https://github.com/loogg/agile_button>
* 邮箱：<2544047213@qq.com>