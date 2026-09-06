#include "L9963E_utils.h"

#include "L9963E.h"
#include "stm32_if.h"
//#include "ntc.h"
//#include "usart.h"
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"		//To be removed
#include "main.h"

//#include "logger_wrapper.h"

L9963E_HandleTypeDef h9l;
volatile uint16_t vcells[CELLS_N];
volatile uint16_t vgpio[GPIOS_N];
volatile uint16_t vtot;
volatile uint32_t vsumbatt;
volatile static float array[4];

const L9963E_IfTypeDef interface = {
    .L9963E_IF_DelayMs = DelayMs,
    .L9963E_IF_GetTickMs = GetTickMs,
    .L9963E_IF_GPIO_ReadPin = GPIO_ReadPin,
    .L9963E_IF_GPIO_WritePin = GPIO_WritePin,
    .L9963E_IF_SPI_Receive = SPI_Receive,
    .L9963E_IF_SPI_Transmit = SPI_Transmit
};

void L9963E_utils_init(void) {
	L9963E_StatusTypeDef status;
	L9963E_BurstUnionTypeDef burst;

	L9963E_RegisterUnionTypeDef GPIOCONFIG;
	GPIOCONFIG.generic = L9963E_GPIO9_3_CONF_DEFAULT;
	GPIOCONFIG.GPIO9_3_CONF.GPIO9_CONFIG = 0b01;
	GPIOCONFIG.GPIO9_3_CONF.GPIO8_CONFIG = 0b01;
	GPIOCONFIG.GPIO9_3_CONF.GPIO7_CONFIG = 0b01;
	GPIOCONFIG.GPIO9_3_CONF.GPIO6_CONFIG = 0b01;
	GPIOCONFIG.GPIO9_3_CONF.GPIO5_CONFIG = 0b01;
	GPIOCONFIG.GPIO9_3_CONF.GPIO4_CONFIG = 0b01;
	GPIOCONFIG.GPIO9_3_CONF.GPIO3_CONFIG = 0b01;
	GPIOCONFIG.GPIO9_3_CONF.GPIO7_WUP_EN = 0;

	L9963E_RegisterUnionTypeDef THRESH;	//Same for both individual cell and total pack config registers
	THRESH.generic = L9963E_VCELL_THRESH_UV_OV_DEFAULT;
	THRESH.VCELL_THRESH_UV_OV.threshVcellOV = 0b11111111;	//About 5 and 70 V
	THRESH.VCELL_THRESH_UV_OV.threshVcellUV = 0;

	L9963E_init(&h9l, interface, 1);
	L9963E_addressing_procedure(&h9l, 0b11, 0, 0, 1);
	L9963E_setCommTimeout(&h9l, _2048MS, L9963E_DEVICE_BROADCAST, 0);	//Easier to debug with longer timeout
	L9963E_set_enabled_cells(&h9l, 0x1, ENABLED_CELLS);
	status = L9963E_DRV_reg_write(&(h9l.drv_handle), 1, L9963E_GPIO9_3_CONF_ADDR, &GPIOCONFIG, 10, 0);
	status = L9963E_DRV_reg_write(&(h9l.drv_handle), 1, L9963E_VCELL_THRESH_UV_OV_ADDR, &THRESH, 10, 0);
	status = L9963E_DRV_reg_write(&(h9l.drv_handle), 1, L9963E_VBATT_SUM_TH_ADDR, &THRESH, 10, 0);
	//status = L9963E_enable_vref(&h9l, L9963E_DEVICE_BROADCAST, 0);	//Used for NTCs


//Debug


	L9963E_RegisterUnionTypeDef read;

	L9963E_DRV_wakeup(&(h9l.drv_handle));
    status = L9963E_DRV_reg_read(&(h9l.drv_handle), 1, L9963E_CSA_GPIO_MSK_ADDR, &read, 10, 0);






    //Read cell voltages
    while (1) {
	L9963E_RegisterUnionTypeDef adcv_conv_reg;
	adcv_conv_reg.generic = L9963E_ADCV_CONV_DEFAULT;
	adcv_conv_reg.ADCV_CONV.SOC = 1;
	adcv_conv_reg.ADCV_CONV.ADC_FILTER_SOC = 0b001;
	adcv_conv_reg.ADCV_CONV.CONF_CYCLIC_EN = 0;
    L9963E_DRV_wakeup(&(h9l.drv_handle));
    status = L9963E_DRV_reg_write(&(h9l.drv_handle), 1, L9963E_ADCV_CONV_ADDR, &adcv_conv_reg, 10, 1);
    HAL_Delay(10);
    L9963E_DRV_burst_cmd(&(h9l.drv_handle), 0x1, _0x78BurstCmd, &burst, 18, 100);

    array[0] = burst._0x78.Frame1_14[0].VCell* 89e-3f;
    array[1] = burst._0x78.Frame1_14[1].VCell* 89e-3f;
    array[2] = burst._0x78.Frame1_14[12].VCell* 89e-3f;
    array[3] = burst._0x78.Frame1_14[13].VCell* 89e-3f;

	L9963E_DRV_wakeup(&(h9l.drv_handle));
    status = L9963E_DRV_reg_read(&(h9l.drv_handle), 1, L9963E_ADCV_CONV_ADDR, &read, 10, 1);

	L9963E_DRV_wakeup(&(h9l.drv_handle));
	L9963E_DRV_burst_cmd(&(h9l.drv_handle), 0x1, _0x7ABurstCmd, &burst, 13, 100);
	read = read;
    }
}

void L9963E_utils_read_cells(uint8_t read_gpio) {
  L9963E_StatusTypeDef e;
  uint8_t c_done;
  L9963E_StatusTypeDef status;
  L9963E_BurstUnionTypeDef data;

  L9963E_DRV_wakeup(&(h9l.drv_handle));
  status = L9963E_start_conversion(&h9l, 0x1, 0b111, read_gpio ? L9963E_GPIO_CONV : 0);
//  L9963E_DRV_burst_cmd(&(h9l.drv_handle), 0x1, _0x7ABurstCmd, &data, 13, 100);

  do {
    L9963E_poll_conversion(&h9l, 0x1, &c_done);
  } while (!c_done);

  
  uint16_t voltage = 0;
  uint8_t d_rdy = 0;

  do {
	  L9963E_DRV_wakeup(&(h9l.drv_handle));
    e = L9963E_read_cell_voltage(&h9l, 0x1, L9963E_CELL1, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vcells[0] = voltage;

  do {
    e = L9963E_read_cell_voltage(&h9l, 0x1, L9963E_CELL2, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vcells[1] = voltage;

  do {
    e = L9963E_read_cell_voltage(&h9l, 0x1, L9963E_CELL3, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vcells[2] = voltage;

  do {
    e = L9963E_read_cell_voltage(&h9l, 0x1, L9963E_CELL4, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vcells[3] = voltage;

  do {
    e = L9963E_read_cell_voltage(&h9l, 0x1, L9963E_CELL12, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vcells[4] = voltage;

  do {
    e = L9963E_read_cell_voltage(&h9l, 0x1, L9963E_CELL13, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vcells[5] = voltage;

  do {
    e = L9963E_read_cell_voltage(&h9l, 0x1, L9963E_CELL14, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vcells[6] = voltage;

  do {
    e = L9963E_read_batt_voltage(&h9l, 0x1, (uint16_t*)&vtot, (uint32_t*)&vsumbatt);
  } while(e != L9963E_OK);

  if(!read_gpio)
    return;

  do {
    e = L9963E_read_gpio_voltage(&h9l, 0x1, L9963E_GPIO3, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vgpio[0] = voltage;

  do {
    e = L9963E_read_gpio_voltage(&h9l, 0x1, L9963E_GPIO4, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vgpio[1] = voltage;

  do {
    e = L9963E_read_gpio_voltage(&h9l, 0x1, L9963E_GPIO5, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vgpio[2] = voltage;

  do {
    e = L9963E_read_gpio_voltage(&h9l, 0x1, L9963E_GPIO6, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vgpio[3] = voltage;

  do {
    e = L9963E_read_gpio_voltage(&h9l, 0x1, L9963E_GPIO7, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vgpio[4] = voltage;

  do {
    e = L9963E_read_gpio_voltage(&h9l, 0x1, L9963E_GPIO8, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vgpio[5] = voltage;

  do {
    e = L9963E_read_gpio_voltage(&h9l, 0x1, L9963E_GPIO9, &voltage, &d_rdy);
  } while(e != L9963E_OK || !d_rdy);
  vgpio[6] = voltage;

  //ntc_set_ext_data((uint16_t*)vgpio, GPIOS_N, 0);	//Commented out original code
}

uint16_t const* L9963E_utils_get_gpios(uint8_t *len) {
  if(len)
    *len = GPIOS_N;
  return (uint16_t*)vgpio;
}

uint16_t const* L9963E_utils_get_cells(uint8_t *len) {
  if(len)
    *len = CELLS_N;
  return (uint16_t*)vcells;
}

float L9963E_utils_get_cell_mv(uint8_t index) {
  return vcells[index] * 89e-3f;
}

void L9963E_utils_get_batt_mv(float *v_tot, float *v_sum) {
  *v_tot = vtot * 1.33f;
  *v_sum = vsumbatt * 89e-3f;
}
