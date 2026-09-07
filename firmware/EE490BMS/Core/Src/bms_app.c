#include "bms_app.h"
#include "L9963E_utils.h"
#include <stddef.h>

#define BMS_GPIO_VOLTAGE_LSB_V 0.000089f

/*
 * Preliminary software protection thresholds.
 * Final values should be verified against the
 * selected battery cell datasheet.
 */
#define BMS_CELL_UV_THRESHOLD_V 2.50f
#define BMS_CELL_OV_THRESHOLD_V 4.20f

static BMS_VoltageData_t voltageData;
static BMS_TemperatureData_t temperatureData;
static BMS_CurrentData_t currentData;
static BMS_FaultData_t faultData;

/*
 * Tracks whether the L9963E current conversion
 * chain has already been successfully enabled.
 */
static bool currentSenseEnabled = false;


void BMS_App_Init(void)
{
    voltageData.valid = false;
    temperatureData.valid = false;
    currentData.valid = false;

    currentData.rawCode = 0;
    currentData.senseVoltage = 0.0f;
    currentData.packCurrent = 0.0f;

    currentSenseEnabled = false;

    faultData.fault = BMS_VOLTAGE_DATA_INVALID;
    faultData.faultCellIndex = 0U;
    faultData.faultActive = false;
}


bool BMS_App_UpdateVoltages(void)
{
    uint8_t cellCount = 0U;

    /*
     * Perform a normal cell-voltage conversion.
     * Passing 0 means GPIO conversion is not requested.
     */
    L9963E_utils_read_cells(0);

    const uint16_t *rawCells =
        L9963E_utils_get_cells(&cellCount);

    if ((rawCells == NULL) ||
        (cellCount != BMS_CELL_COUNT))
    {
        voltageData.valid = false;

        BMS_App_CheckVoltageFaults();

        return false;
    }

    voltageData.packVoltage = 0.0f;

    for (uint8_t i = 0U;
         i < BMS_CELL_COUNT;
         i++)
    {
        voltageData.cellVoltage[i] =
            L9963E_utils_get_cell_mv(i) / 1000.0f;

        voltageData.packVoltage +=
            voltageData.cellVoltage[i];
    }

    voltageData.minCellVoltage =
        voltageData.cellVoltage[0];

    voltageData.maxCellVoltage =
        voltageData.cellVoltage[0];

    voltageData.minCellIndex = 0U;
    voltageData.maxCellIndex = 0U;

    for (uint8_t i = 1U;
         i < BMS_CELL_COUNT;
         i++)
    {
        if (voltageData.cellVoltage[i] <
            voltageData.minCellVoltage)
        {
            voltageData.minCellVoltage =
                voltageData.cellVoltage[i];

            voltageData.minCellIndex = i;
        }

        if (voltageData.cellVoltage[i] >
            voltageData.maxCellVoltage)
        {
            voltageData.maxCellVoltage =
                voltageData.cellVoltage[i];

            voltageData.maxCellIndex = i;
        }
    }

    voltageData.deltaVoltage =
        voltageData.maxCellVoltage -
        voltageData.minCellVoltage;

    voltageData.valid = true;

    BMS_App_CheckVoltageFaults();

    return true;
}


bool BMS_App_UpdateTemperatureInputs(void)
{
    uint8_t gpioCount = 0U;

    /*
     * Passing 1 requests GPIO conversion in addition
     * to the normal cell-voltage conversion.
     */
    L9963E_utils_read_cells(1);

    const uint16_t *rawGpios =
        L9963E_utils_get_gpios(&gpioCount);

    if ((rawGpios == NULL) ||
        (gpioCount != BMS_TEMP_CHANNEL_COUNT))
    {
        temperatureData.valid = false;

        return false;
    }

    for (uint8_t i = 0U;
         i < BMS_TEMP_CHANNEL_COUNT;
         i++)
    {
        temperatureData.raw[i] =
            rawGpios[i];

        temperatureData.gpioVoltage[i] =
            ((float)rawGpios[i]) *
            BMS_GPIO_VOLTAGE_LSB_V;
    }

    temperatureData.valid = true;

    return true;
}


bool BMS_App_UpdateCurrent(void)
{
    int32_t rawCurrent = 0;

    /*
     * Enable the L9963E current conversion chain
     * the first time current measurement is requested.
     *
     * If communication fails, leave the flag false
     * so the application will try again next time.
     */
    if (!currentSenseEnabled)
    {
        if (!L9963E_utils_enable_current_sense())
        {
            currentData.valid = false;

            return false;
        }

        currentSenseEnabled = true;
    }

    /*
     * Read the continuously updated signed 18-bit
     * current ADC measurement.
     */
    if (!L9963E_utils_read_current_raw(&rawCurrent))
    {
        currentData.valid = false;

        return false;
    }

    currentData.rawCode = rawCurrent;

    /*
     * Convert ADC counts into the differential voltage
     * across the external current-sense shunt.
     *
     * L9963E current ADC resolution:
     * 1.33 microvolts per count.
     */
    currentData.senseVoltage =
        ((float)rawCurrent) *
        L9963_CURRENT_LSB_V;

    /*
     * Ohm's law:
     *
     * I = Vshunt / Rshunt
     *
     * BMS_CURRENT_SHUNT_OHMS is currently a temporary
     * configuration value and must be updated when the
     * team's final shunt resistor is selected.
     */
    currentData.packCurrent =
        currentData.senseVoltage /
        BMS_CURRENT_SHUNT_OHMS;

    /*
     * Current polarity depends on the physical orientation
     * of ISENSEP, ISENSEM, and the shunt resistor.
     * Do not assume positive means charge or discharge until
     * the team's schematic/hardware orientation is confirmed.
     */
    currentData.valid = true;

    return true;
}


void BMS_App_CheckVoltageFaults(void)
{
    faultData.fault = BMS_VOLTAGE_OK;
    faultData.faultCellIndex = 0U;
    faultData.faultActive = false;

    /*
     * Voltage protection decisions should only be
     * made when the latest voltage data is valid.
     */
    if (!voltageData.valid)
    {
        faultData.fault =
            BMS_VOLTAGE_DATA_INVALID;

        faultData.faultActive = true;

        return;
    }

    /*
     * Check every cell for an undervoltage or
     * overvoltage condition.
     */
    for (uint8_t i = 0U;
         i < BMS_CELL_COUNT;
         i++)
    {
        if (voltageData.cellVoltage[i] <
            BMS_CELL_UV_THRESHOLD_V)
        {
            faultData.fault =
                BMS_CELL_UNDERVOLTAGE;

            faultData.faultCellIndex = i;
            faultData.faultActive = true;

            return;
        }

        if (voltageData.cellVoltage[i] >
            BMS_CELL_OV_THRESHOLD_V)
        {
            faultData.fault =
                BMS_CELL_OVERVOLTAGE;

            faultData.faultCellIndex = i;
            faultData.faultActive = true;

            return;
        }
    }
}


const BMS_VoltageData_t *BMS_App_GetVoltageData(void)
{
    return &voltageData;
}


const BMS_TemperatureData_t *BMS_App_GetTemperatureData(void)
{
    return &temperatureData;
}


const BMS_CurrentData_t *BMS_App_GetCurrentData(void)
{
    return &currentData;
}


const BMS_FaultData_t *BMS_App_GetFaultData(void)
{
    return &faultData;
}
