#include "drv_gpio.h"

#define RCC_GPIOA       0
#define RCC_GPIOB       1
#define RCC_GPIOC       2
#define RCC_GPIOD       3

struct pin_index
{
    int index;
    uint8_t rcc_gpiox;
    GPIO_TypeDef *GPIOx;
    uint32_t pin;
};

static const struct pin_index pins[] = 
{
    {0, RCC_GPIOA, GPIOA, GPIO_PIN_8},  //D0
    {1, RCC_GPIOD, GPIOD, GPIO_PIN_2},  //D1
    {2, RCC_GPIOA, GPIOA, GPIO_PIN_0},  //WK_UP
    {5, RCC_GPIOB, GPIOB, GPIO_PIN_0},  //WIFI Power
    {6, RCC_GPIOB, GPIOB, GPIO_PIN_1},  //WIFI RST
    {7, RCC_GPIOC, GPIOC, GPIO_PIN_6},  //OLED RES
    {8, RCC_GPIOC, GPIOC, GPIO_PIN_7},  //OLED DC
    {9, RCC_GPIOC, GPIOC, GPIO_PIN_8}   //OLED CS
};

#define ITEM_NUM(items) sizeof(items) / sizeof(items[0])

static const struct pin_index *get_pin(uint8_t pin)
{
    int num = ITEM_NUM(pins);
    for (int i = 0; i < num; i++)
    {
        if(pins[i].index == pin)
            return &pins[i];
    }

    return RT_NULL;
};

void drv_pin_mode(rt_base_t pin, rt_base_t mode)
{
    const struct pin_index *index = get_pin(pin);
    if(index == RT_NULL)
        return;
    
    switch(index->rcc_gpiox)
    {
        case RCC_GPIOA:
            __HAL_RCC_GPIOA_CLK_ENABLE();
        break;

        case RCC_GPIOB:
            __HAL_RCC_GPIOB_CLK_ENABLE();
        break;

        case RCC_GPIOC:
            __HAL_RCC_GPIOC_CLK_ENABLE();
        break;

        case RCC_GPIOD:
            __HAL_RCC_GPIOD_CLK_ENABLE();
        break;

        default:
            return;
    }

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = index->pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    switch(mode)
    {
        case PIN_MODE_OUTPUT:
            GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
        break;

        case PIN_MODE_INPUT:
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
        break;

        case PIN_MODE_INPUT_PULLUP:
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
            GPIO_InitStruct.Pull = GPIO_PULLUP;
        break;

        case PIN_MODE_INPUT_PULLDOWN:
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
            GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        break;

        case PIN_MODE_OUTPUT_OD:
            GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
        break;

        default:
            return;
    }

    HAL_GPIO_Init(index->GPIOx, &GPIO_InitStruct);
}

void drv_pin_write(rt_base_t pin, rt_base_t value)
{
    const struct pin_index *index = get_pin(pin);
    if(index == RT_NULL)
        return;

    HAL_GPIO_WritePin(index->GPIOx, index->pin, (GPIO_PinState)(value ? 1 : 0));
}

int drv_pin_read(rt_base_t pin)
{
    int value = PIN_LOW;
    const struct pin_index *index = get_pin(pin);
    if(index == RT_NULL)
        return value;

    value = HAL_GPIO_ReadPin(index->GPIOx, index->pin);

    return value;
}

static rt_size_t _pin_read(usr_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    if(pos < 0)
        return 0;
    
    int *status = (int *)buffer;
    if((status == RT_NULL) || (size != sizeof(int)))
        return 0;
    
    *status = drv_pin_read(pos);
    return size;
}

static rt_size_t _pin_write(usr_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    if(pos < 0)
        return 0;
    
    int *status = (int *)buffer;
    if((status == RT_NULL) || (size != sizeof(int)))
        return 0;
    
    drv_pin_write(pos, *status);
    return size;
}

static rt_err_t _pin_control(usr_device_t dev, int cmd, void *args)
{
    struct usr_device_pin_mode *mode = (struct usr_device_pin_mode *)args;
    if(mode == RT_NULL)
        return -RT_ERROR;
    
    drv_pin_mode(mode->pin, mode->mode);
    
    return RT_EOK;
}

static struct usr_device_pin _hw_pin = {0};

static int drv_hw_pin_init(void)
{
    rt_memset(&_hw_pin, 0, sizeof(struct usr_device_pin));

    _hw_pin.parent.init = RT_NULL;
    _hw_pin.parent.read = _pin_read;
    _hw_pin.parent.write = _pin_write;
    _hw_pin.parent.control = _pin_control;

    usr_device_register(&(_hw_pin.parent), "pin");

    return RT_EOK;
}
INIT_BOARD_EXPORT(drv_hw_pin_init);
