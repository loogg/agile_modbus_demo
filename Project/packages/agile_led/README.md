# agile_led

## 1、介绍

agile_led是基于RT-Thread实现的led软件包，提供led操作的API。

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

agile_led package 遵循 LGPLv2.1 许可，详见 `LICENSE` 文件。

### 1.4 依赖

- RT-Thread 3.0+
- RT-Thread 4.0+

## 2、如何打开 agile_led

使用 agile_led package 需要在 RT-Thread 的包管理器中选择它，具体路径如下：

```
RT-Thread online packages
    peripheral libraries and drivers --->
        [*] agile_led: A agile led package
```

然后让 RT-Thread 的包管理器自动更新，或者使用 `pkgs --update` 命令更新包到 BSP 中。

## 3、使用 agile_led

在打开 agile_led package 后，当进行 bsp 编译时，它会被加入到 bsp 工程中进行编译。

### 3.1、API说明

1. 创建led对象

```C
agile_led_t *agile_led_create(rt_base_t pin, rt_base_t active_logic, const char *light_mode, int32_t loop_cnt);
```

|参数|注释|
|----|----|
|pin|控制led的引脚|
|active_logic|led有效电平(PIN_HIGH/PIN_LOW)|
|light_mode|闪烁模式字符串|
|loop_cnt|循环次数(负数为永久循环)|

|返回|注释|
|----|----|
|!=RT_NULL|agile_led对象指针|
|RT_NULL|异常|

2. 删除led对象

```C
int agile_led_delete(agile_led_t *led);
```

|参数|注释|
|----|----|
|led|led对象指针|

|返回|注释|
|----|----|
|RT_EOK|成功|

3. 启动led对象,根据设置的模式执行动作

```C
int agile_led_start(agile_led_t *led);
```

|参数|注释|
|----|----|
|led|led对象指针|

|返回|注释|
|----|----|
|RT_EOK|成功|
|!=RT_OK|异常|

4. 停止led对象

```C
int agile_led_stop(agile_led_t *led);
```

|参数|注释|
|----|----|
|led|led对象指针|

|返回|注释|
|----|----|
|RT_EOK|成功|

5. 设置led对象的模式

```C
int agile_led_set_light_mode(agile_led_t *led, const char *light_mode, int32_t loop_cnt);
```

|参数|注释|
|----|----|
|led|led对象指针|
|light_mode|闪烁模式字符串|
|loop_cnt|循环次数(负数为永久循环)|

|返回|注释|
|----|----|
|RT_EOK|成功|
|!=RT_EOK|异常|

6. 设置led对象操作完成的回调函数

```C
int agile_led_set_compelete_callback(agile_led_t *led, void (*compelete)(agile_led_t *led));
```

|参数|注释|
|----|----|
|led|led对象指针|
|compelete|操作完成回调函数|

|返回|注释|
|----|----|
|RT_EOK|成功|

7. led对象电平翻转

```C
void agile_led_toggle(agile_led_t *led);
```

|参数|注释|
|----|----|
|led|led对象指针|

8. led对象亮

```C
void agile_led_on(agile_led_t *led);
```

|参数|注释|
|----|----|
|led|led对象指针|

9. led对象灭

```C
void agile_led_off(agile_led_t *led);
```

|参数|注释|
|----|----|
|led|led对象指针|

### 3.2、示例

使用示例在 [examples](./examples) 下。

## 4、注意事项

1. 调用 `agile_led_create` API创建完led对象后，调用其他API确保led对象创建成功，否则被断言。
2. 调用 `agile_led_create` 和 `agile_led_set_light_mode` API时，参数 `light_mode` 可以为RT_NULL。
3. `light_mode` 确保时字符串形式，如 `"100,50,10,60"` 或 `"100,50,10,60,"` ,只支持正整数，按照亮灭亮灭...规律。

## 5、联系方式 & 感谢

* 维护：马龙伟
* 主页：<https://github.com/loogg/agile_led>
* 邮箱：<2544047213@qq.com>