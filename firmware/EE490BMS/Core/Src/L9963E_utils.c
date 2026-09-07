#include "L9963E_utils.h"

#include "L9963E.h"
#include "stm32_if.h"
#include "main.h"

/*
 * Maximum amount of time that one measurement operation is
 * allowed to wait for the L9963E.
 *
 * This prevents a communication or hardware problem from
 * permanently blocking the application.
 */
#define L9963E_UTILS_MEASUREMENT_TIMEOUT_MS 100U

//Delay between unsuccessful polling attempts.
#define L9963E_UTILS_POLL_DELAY_MS 1U


// Globals
L9963E_HandleTypeDef h9l;
volatile uint16_t vcells[CELLS_N];
volatile uint16_t vgpio[GPIOS_N];
volatile uint16_t vtot;
volatile uint32_t vsumbatt;

const L9963E_IfTypeDef interface = {
    .L9963E_IF_DelayMs = DelayMs,
    .L9963E_IF_GetTickMs = GetTickMs,
    .L9963E_IF_GPIO_ReadPin = GPIO_ReadPin,
    .L9963E_IF_GPIO_WritePin = GPIO_WritePin,
    .L9963E_IF_SPI_Receive = SPI_Receive,
    .L9963E_IF_SPI_Transmit = SPI_Transmit
};


/* ===================== Private helper functions ===================== */

/*
 * Wait for the current ADC conversion to finish.
 *
 * Returns:
 *   1 = conversion completed
 *   0 = communication failure or timeout
 */
static uint8_t L9963E_utils_wait_for_conversion(void)
{
    uint32_t startTime = GetTickMs();
    uint8_t conversionDone = 0U;

    while ((GetTickMs() - startTime) < L9963E_UTILS_MEASUREMENT_TIMEOUT_MS)
    {
        L9963E_StatusTypeDef status = L9963E_poll_conversion(&h9l, 0x1, &conversionDone);

        if (status != L9963E_OK) return 0U;
        if (conversionDone) return 1U;

        DelayMs(L9963E_UTILS_POLL_DELAY_MS);
    }

    return 0U;
}

// Read one cell voltage and wait until fresh data is available.
static uint8_t L9963E_utils_read_cell_with_timeout(L9963E_CellsTypeDef cell, uint16_t *voltage)
{
    if (voltage == NULL) return 0U;

    uint32_t startTime = GetTickMs();

    while ((GetTickMs() - startTime) < L9963E_UTILS_MEASUREMENT_TIMEOUT_MS)
    {
        uint8_t dataReady = 0U;

        L9963E_DRV_wakeup(&(h9l.drv_handle));

        L9963E_StatusTypeDef status;
        status = L9963E_read_cell_voltage(&h9l, 0x1, cell, voltage, &dataReady);

        if ((status == L9963E_OK) && (dataReady != 0U))
        {
            return 1U;
        }

        DelayMs(L9963E_UTILS_POLL_DELAY_MS);
    }

    return 0U;
}

//Read one GPIO ADC channel and wait until fresh data is available.
static uint8_t L9963E_utils_read_gpio_with_timeout(L9963E_GpiosTypeDef gpio, uint16_t *voltage)
{
    if (voltage == NULL) return 0U;

    uint32_t startTime = GetTickMs();

    while ((GetTickMs() - startTime) < L9963E_UTILS_MEASUREMENT_TIMEOUT_MS)
    {
        uint8_t dataReady = 0U;

        L9963E_DRV_wakeup(&(h9l.drv_handle));

        L9963E_StatusTypeDef status = L9963E_read_gpio_voltage(&h9l, 0x1, gpio, voltage, &dataReady);

        if ((status == L9963E_OK) && (dataReady != 0U))
        {
            return 1U;
        }

        DelayMs(L9963E_UTILS_POLL_DELAY_MS);
    }

    return 0U;
}


//Read the total battery voltage with a bounded timeout.
static uint8_t L9963E_utils_read_battery_with_timeout(uint16_t *totalVoltage, uint32_t *sumVoltage)
{
    if ((totalVoltage == NULL) || (sumVoltage == NULL)) return 0U;

    uint32_t startTime = GetTickMs();

    while ((GetTickMs() - startTime) < L9963E_UTILS_MEASUREMENT_TIMEOUT_MS)
    {
        L9963E_StatusTypeDef status;
        status = L9963E_read_batt_voltage(&h9l, 0x1, totalVoltage, sumVoltage);

        if (status == L9963E_OK) return 1U;

        DelayMs(L9963E_UTILS_POLL_DELAY_MS);
    }

    return 0U;
}

uint8_t L9963E_utils_init(void)
{
    L9963E_StatusTypeDef status;		// Debug variable to check read/write functions

    //Disable all GPIOs on AFE
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

    //Set UV/OV thresholds as wide as possible for individual cell and full stack
    L9963E_RegisterUnionTypeDef THRESH;
    THRESH.generic = L9963E_VCELL_THRESH_UV_OV_DEFAULT;
    THRESH.VCELL_THRESH_UV_OV.threshVcellOV = 0b11111111;
    THRESH.VCELL_THRESH_UV_OV.threshVcellUV = 0;

    status = L9963E_init(&h9l, interface, 1);
    status = L9963E_addressing_procedure(&h9l, 0b11, 0, 0, 1);
    status = L9963E_setCommTimeout(&h9l, _2048MS, L9963E_DEVICE_BROADCAST, 0);	//Set to longer to help with debugging
    status = L9963E_set_enabled_cells(&h9l, 0x1, ENABLED_CELLS);
    status = L9963E_DRV_reg_write(&(h9l.drv_handle), 0x1, L9963E_GPIO9_3_CONF_ADDR, &GPIOCONFIG, 10, 0);
    status = L9963E_DRV_reg_write(&(h9l.drv_handle), 0x1, L9963E_VCELL_THRESH_UV_OV_ADDR, &THRESH, 10, 0);
    status = L9963E_DRV_reg_write(&(h9l.drv_handle), 0x1, L9963E_VBATT_SUM_TH_ADDR, &THRESH, 10, 0);

    return status;
}

/* ===================== Voltage / GPIO acquisition ===================== */

uint8_t L9963E_utils_read_cells(uint8_t read_gpio)
{
    /*
     * Measurements are collected into temporary storage first.
     *
     * The public measurement arrays are only updated after
     * every requested read succeeds. This prevents the rest
     * of the BMS from seeing a half-old / half-new data set.
     */
    uint16_t newCells[CELLS_N];
    uint16_t newGpios[GPIOS_N];
    uint16_t newVtot = 0U;
    uint32_t newVsumbatt = 0U;
    L9963E_StatusTypeDef status;

    //Begin an on-demand ADC conversion.
    L9963E_DRV_wakeup(&(h9l.drv_handle));

    status = L9963E_start_conversion(&h9l, 0x1, 0b111, read_gpio ? L9963E_GPIO_CONV : 0U);

    if (status != L9963E_OK) return 0U;

    //Wait for conversion completion without allowing the MCU to hang forever.
    if (!L9963E_utils_wait_for_conversion()) return 0U;

    //Acquire the seven cell channels used by the project's 7S application mapping.
    if (!L9963E_utils_read_cell_with_timeout(L9963E_CELL1, &newCells[0])) return 0U;
    if (!L9963E_utils_read_cell_with_timeout(L9963E_CELL2, &newCells[1])) return 0U;
    if (!L9963E_utils_read_cell_with_timeout(L9963E_CELL3, &newCells[2])) return 0U;
    if (!L9963E_utils_read_cell_with_timeout(L9963E_CELL4, &newCells[3])) return 0U;
    if (!L9963E_utils_read_cell_with_timeout(L9963E_CELL12, &newCells[4])) return 0U;
    if (!L9963E_utils_read_cell_with_timeout(L9963E_CELL13, &newCells[5])) return 0U;
    if (!L9963E_utils_read_cell_with_timeout(L9963E_CELL14, &newCells[6])) return 0U;

    //Read total battery voltage.
    if (!L9963E_utils_read_battery_with_timeout(&newVtot, &newVsumbatt))
    {
        return 0U;
    }

    //If GPIO/temperature measurements were requested, acquire GPIO3 through GPIO9.
    if (read_gpio)
    {
        if (!L9963E_utils_read_gpio_with_timeout(L9963E_GPIO3, &newGpios[0])) return 0U;
        if (!L9963E_utils_read_gpio_with_timeout(L9963E_GPIO4, &newGpios[1])) return 0U;
        if (!L9963E_utils_read_gpio_with_timeout(L9963E_GPIO5, &newGpios[2])) return 0U;
        if (!L9963E_utils_read_gpio_with_timeout(L9963E_GPIO6, &newGpios[3])) return 0U;
        if (!L9963E_utils_read_gpio_with_timeout(L9963E_GPIO7, &newGpios[4])) return 0U;
        if (!L9963E_utils_read_gpio_with_timeout(L9963E_GPIO8, &newGpios[5])) return 0U;
        if (!L9963E_utils_read_gpio_with_timeout(L9963E_GPIO9, &newGpios[6])) return 0U;
    }

    /*
     * The complete requested measurement set succeeded.
     *
     * Publish the new values atomically from the
     * application's point of view.
     */
    for (uint8_t i = 0U; i < CELLS_N; i++)
    {
        vcells[i] = newCells[i];
    }

    vtot = newVtot;
    vsumbatt = newVsumbatt;

    if (read_gpio)
    {
        for (uint8_t i = 0U; i < GPIOS_N; i++)
        {
            vgpio[i] = newGpios[i];
        }
    }
    return 1U;
}


/* ===================== Measurement access ===================== */

uint16_t const *L9963E_utils_get_gpios(uint8_t *len)
{
    if (len)
    {
        *len = GPIOS_N;
    }

    return (uint16_t *)vgpio;
}


uint16_t const *L9963E_utils_get_cells(uint8_t *len)
{
    if (len)
    {
        *len = CELLS_N;
    }

    return (uint16_t *)vcells;
}


float L9963E_utils_get_cell_mv(uint8_t index)
{
    return vcells[index] * 89e-3f;
}


void L9963E_utils_get_batt_mv(float *v_tot, float *v_sum)
{
    *v_tot = vtot * 1.33f;
    *v_sum = vsumbatt * 89e-3f;
}


/* ===================== Current sensing ===================== */

uint8_t L9963E_utils_enable_current_sense(void)
{
    L9963E_RegisterUnionTypeDef csaConfig;
    L9963E_StatusTypeDef status;

    //Read the existing CSA configuration first as to not overwrite any unrelated masks or thresholds.
    L9963E_DRV_wakeup(&(h9l.drv_handle));

    status = L9963E_DRV_reg_read(&(h9l.drv_handle), 0x1, L9963E_CSA_GPIO_MSK_ADDR, &csaConfig, 10, 0);

    if (status != L9963E_OK) return 0U;

    csaConfig.CSA_GPIO_MSK.CoulombCounter_en = 1;

    L9963E_DRV_wakeup(&(h9l.drv_handle));
    status = L9963E_DRV_reg_write(&(h9l.drv_handle), 0x1, L9963E_CSA_GPIO_MSK_ADDR, &csaConfig, 10, 0);

    if (status != L9963E_OK) return 0U;

    return 1U;
}


uint8_t L9963E_utils_read_current_raw(int32_t *raw_current)
{
    L9963E_RegisterUnionTypeDef currentReg;
    L9963E_StatusTypeDef status;

    if (raw_current == NULL) return 0U;

    L9963E_DRV_wakeup(&(h9l.drv_handle));

    status = L9963E_DRV_reg_read(&(h9l.drv_handle), 0x1, L9963E_Ibattery_calib_ADDR, &currentReg, 10, 0);

    if (status != L9963E_OK) return 0U;


    //Ibattery_calib contains an 18-bit two's-complement current value in bits 17:0.
    uint32_t raw18 = currentReg.generic & 0x3FFFFUL;


    //Sign-extend the 18-bit value to a normal signed 32-bit integer.
    // Bit 17 is the sign bit.
    if (raw18 & 0x20000UL)
    {
        raw18 |= 0xFFFC0000UL;
    }

    *raw_current = (int32_t)raw18;

    return 1U;
}


/* ===================== Coulomb counting ===================== */

uint8_t L9963E_utils_read_coulomb_counter(L9963E_CoulombData_t *data)
{
    L9963E_BurstUnionTypeDef burstData = {0};
    L9963E_StatusTypeDef status;

    if (data == NULL) return 0U;

    //The current-sense / Coulomb Counter chain must already be enabled before meaningful data exists.
    L9963E_DRV_wakeup(&(h9l.drv_handle));


    status = L9963E_DRV_burst_cmd(&(h9l.drv_handle), 0x1,  _0x7BBurstCmd, &burstData, L9963E_BURST_0x7B_LEN, 20);


    if (status != L9963E_OK) return 0U;

    /*
     * Frame 1 contains the number of current samples
     * accumulated during this interval.
     */
    data->sampleCount = (uint16_t) burstData._0x7B.Frame1.CoulombCntTime;
    data->overflow = (uint8_t) burstData._0x7B.Frame1.CoCouOvF;

    // The Coulomb accumulator is one signed 32-bit two's-complement value split into two 16-bit registers.
    uint32_t accumulatorRaw = ((uint32_t) burstData._0x7B.Frame2.CoulombCounter_msb << 16) |
        ((uint32_t) burstData._0x7B.Frame3.CoulombCounter_lsb);

    data->accumulatorCode = (int32_t)accumulatorRaw;

    // The two instantaneous-current fields are signed 18-bit two's-complement values.
    uint32_t synchRaw = (uint32_t) burstData._0x7B.Frame4.CUR_INST_synch;

    if (synchRaw & 0x20000UL)
    {
        synchRaw |= 0xFFFC0000UL;
    }

    data->currentSynchRaw = (int32_t)synchRaw;

    uint32_t calibRaw = (uint32_t) burstData._0x7B.Frame5.CUR_INST_calib;

    if (calibRaw & 0x20000UL)
    {
        calibRaw |= 0xFFFC0000UL;
    }

    data->currentCalibRaw = (int32_t)calibRaw;

    return 1U;
}
