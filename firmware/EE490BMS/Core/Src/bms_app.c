#include "bms_app.h"
#include "L9963E_utils.h"
#include <stddef.h>

#define BMS_GPIO_VOLTAGE_LSB_V 0.000089f

static BMS_VoltageData_t voltageData;
static BMS_TemperatureData_t temperatureData;

void BMS_App_Init(void)
{
    voltageData.valid = false;
    temperatureData.valid = false;
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

    if ((rawCells == NULL) || (cellCount != BMS_CELL_COUNT))
    {
        voltageData.valid = false;
        return false;
    }

    voltageData.packVoltage = 0.0f;

    for (uint8_t i = 0U; i < BMS_CELL_COUNT; i++)
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

    for (uint8_t i = 1U; i < BMS_CELL_COUNT; i++)
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
        temperatureData.raw[i] = rawGpios[i];

        temperatureData.gpioVoltage[i] =
            ((float)rawGpios[i]) *
            BMS_GPIO_VOLTAGE_LSB_V;
    }

    temperatureData.valid = true;

    return true;
}

const BMS_VoltageData_t *BMS_App_GetVoltageData(void)
{
    return &voltageData;
}

const BMS_TemperatureData_t *BMS_App_GetTemperatureData(void)
{
    return &temperatureData;
}
