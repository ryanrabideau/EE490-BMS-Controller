#ifndef BMS_APP_H
#define BMS_APP_H

#include <stdint.h>
#include <stdbool.h>

/* ===================== Configuration ===================== */

#define BMS_CELL_COUNT           7U
#define BMS_TEMP_CHANNEL_COUNT   7U

/*
 * Temporary shunt resistance value.
 * This must be confirmed once the team's final shunt resistor
 * and maximum pack current are selected.
 */
#define BMS_CURRENT_SHUNT_OHMS   0.0001f

/* ===================== Voltage Data ===================== */

typedef struct
{
    float cellVoltage[BMS_CELL_COUNT];

    float packVoltage;

    float minCellVoltage;
    float maxCellVoltage;
    float deltaVoltage;

    uint8_t minCellIndex;
    uint8_t maxCellIndex;

    bool valid;

} BMS_VoltageData_t;

/* ===================== Temperature Data ===================== */

typedef struct
{
    uint16_t raw[BMS_TEMP_CHANNEL_COUNT];

    float gpioVoltage[BMS_TEMP_CHANNEL_COUNT];

    bool valid;

} BMS_TemperatureData_t;

/* ===================== Current Data ===================== */

typedef struct
{
    /*
     * Signed 18-bit current ADC value from the L9963E.
     */
    int32_t rawCode;

    /*
     * Differential voltage measured across the shunt resistor.
     */
    float senseVoltage;

    /*
     * Calculated battery pack current in amperes.
     */
    float packCurrent;

    bool valid;

} BMS_CurrentData_t;

/* ===================== Voltage Faults ===================== */

typedef enum
{
    BMS_VOLTAGE_OK = 0,

    BMS_CELL_UNDERVOLTAGE,

    BMS_CELL_OVERVOLTAGE,

    BMS_VOLTAGE_DATA_INVALID

} BMS_VoltageFault_t;

typedef struct
{
    BMS_VoltageFault_t fault;

    uint8_t faultCellIndex;

    bool faultActive;

} BMS_FaultData_t;

/* ===================== Application Functions ===================== */

void BMS_App_Init(void);

bool BMS_App_UpdateVoltages(void);

bool BMS_App_UpdateTemperatureInputs(void);

bool BMS_App_UpdateCurrent(void);

void BMS_App_CheckVoltageFaults(void);

/* ===================== Data Access ===================== */

const BMS_VoltageData_t *BMS_App_GetVoltageData(void);

const BMS_TemperatureData_t *BMS_App_GetTemperatureData(void);

const BMS_CurrentData_t *BMS_App_GetCurrentData(void);

const BMS_FaultData_t *BMS_App_GetFaultData(void);

#endif
