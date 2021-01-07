#include "oled.h"
#include "drv_gpio.h"
#include "stm32f1xx_hal.h"
#include <rthw.h>

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME "oled"
#define DBG_LEVEL DBG_INFO
#include <rtdbg.h>

#define OLED_RES_PIN        7
#define OLED_DC_PIN         8
#define OLED_CS_PIN         9

SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi2_tx;

static rt_uint8_t oled_gram[8][128] = {0};
static struct usr_device_oled oled_device = {0};

static void _oled_hw_init(void)
{
    static rt_uint8_t init_ok = 0;

    rt_base_t level = rt_hw_interrupt_disable();

    if(init_ok)
        HAL_SPI_DeInit(&hspi2);
    
    drv_pin_mode(OLED_RES_PIN, PIN_MODE_OUTPUT);
    drv_pin_write(OLED_RES_PIN, PIN_HIGH);

    drv_pin_mode(OLED_DC_PIN, PIN_MODE_OUTPUT);
    drv_pin_write(OLED_DC_PIN, PIN_HIGH);

    drv_pin_mode(OLED_CS_PIN, PIN_MODE_OUTPUT);
    drv_pin_write(OLED_CS_PIN, PIN_HIGH);

    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 10;
    HAL_SPI_Init(&hspi2);
    HAL_SPI_Abort(&hspi2);

    init_ok = 1;

    rt_hw_interrupt_enable(level);
}

static void _oled_write_cmd(rt_uint8_t cmd)
{
    drv_pin_write(OLED_DC_PIN, PIN_LOW);
    drv_pin_write(OLED_CS_PIN, PIN_LOW);

    HAL_SPI_Abort(&hspi2);
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 1000);

    drv_pin_write(OLED_CS_PIN, PIN_HIGH);
    drv_pin_write(OLED_DC_PIN, PIN_HIGH);
}

static void _oled_refresh_gram(void)
{
    drv_pin_write(OLED_DC_PIN, PIN_HIGH);
    drv_pin_write(OLED_CS_PIN, PIN_LOW);

    HAL_SPI_Abort(&hspi2);
    HAL_SPI_Transmit_DMA(&hspi2, oled_gram[0], sizeof(oled_gram));
}

static rt_err_t _oled_init(usr_device_t dev)
{
    _oled_hw_init();

    drv_pin_write(OLED_RES_PIN, PIN_HIGH);
    rt_thread_mdelay(100);
    drv_pin_write(OLED_RES_PIN, PIN_LOW);
    rt_thread_mdelay(200);
    drv_pin_write(OLED_RES_PIN, PIN_HIGH);
    rt_thread_mdelay(100);

    _oled_write_cmd(0xAE);//--turn off oled panel
	_oled_write_cmd(0x00);//---set low column address
	_oled_write_cmd(0x10);//---set high column address
	_oled_write_cmd(0x40);//--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
	_oled_write_cmd(0x81);//--set contrast control register
	_oled_write_cmd(0xCF); // Set SEG Output Current Brightness
	_oled_write_cmd(0xA1);//--Set SEG/Column Mapping     0xa0×óÓÒ·´ÖÃ 0xa1Õý³£
	_oled_write_cmd(0xC8);//Set COM/Row Scan Direction   0xc0ÉÏÏÂ·´ÖÃ 0xc8Õý³£
	_oled_write_cmd(0xA6);//--set normal display
	_oled_write_cmd(0xA8);//--set multiplex ratio(1 to 64)
	_oled_write_cmd(0x3f);//--1/64 duty
	_oled_write_cmd(0xD3);//-set display offset	Shift Mapping RAM Counter (0x00~0x3F)
	_oled_write_cmd(0x00);//-not offset
	_oled_write_cmd(0xd5);//--set display clock divide ratio/oscillator frequency
	_oled_write_cmd(0x80);//--set divide ratio, Set Clock as 100 Frames/Sec
	_oled_write_cmd(0xD9);//--set pre-charge period
	_oled_write_cmd(0xF1);//Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
	_oled_write_cmd(0xDA);//--set com pins hardware configuration
	_oled_write_cmd(0x12);
	_oled_write_cmd(0xDB);//--set vcomh
	_oled_write_cmd(0x40);//Set VCOM Deselect Level
	_oled_write_cmd(0x20);//-Set Horizontal addressing mode (0x00/0x01/0x02)
	_oled_write_cmd(0x00);//
	_oled_write_cmd(0x8D);//--set Charge Pump enable/disable
	_oled_write_cmd(0x14);//--set(0x10) disable
	_oled_write_cmd(0xA4);// Disable Entire Display On (0xa4/0xa5)
	_oled_write_cmd(0xA6);// Disable Inverse Display On (0xa6/a7) 
	_oled_write_cmd(0xAF);//--turn on oled panel
	
    _oled_refresh_gram();
    
    
    return RT_EOK;
}

static rt_err_t _oled_control(usr_device_t dev, int cmd, void *args)
{
    rt_err_t result = -RT_ERROR;

    switch(cmd)
    {
        case USR_DEVICE_OLED_CMD_RECT_UPDATE:
        {
            _oled_refresh_gram();
            result = RT_EOK;
        }
        break;

        default:
        break;
    }

    return result;
}


static int _hw_oled_init(void)
{
    rt_memset(&oled_device, 0, sizeof(struct usr_device_oled));

    oled_device.parent.init = _oled_init;
    oled_device.parent.read = RT_NULL;
    oled_device.parent.write = RT_NULL;
    oled_device.parent.control = _oled_control;

    usr_device_register(&(oled_device.parent), "oled");

    return RT_EOK;
}
INIT_BOARD_EXPORT(_hw_oled_init);

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    drv_pin_write(OLED_CS_PIN, PIN_HIGH);

    if(oled_device.parent.tx_complete)
        oled_device.parent.tx_complete(&(oled_device.parent), RT_NULL);
}

void DMA1_Channel5_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&hdma_spi2_tx);

    /* leave interrupt */
    rt_interrupt_leave();
}
