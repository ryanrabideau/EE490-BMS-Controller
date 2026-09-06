#ifndef BMS_APP_H
#define BMS_APP_H

#include <stdint.h>
#include <stdbool.h>

#define BMS_CELL_COUNT          7U
#define BMS_TEMP_CHANNEL_COUNT  7U

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

typedef struct
{
    uint16_t raw[BMS_TEMP_CHANNEL_COUNT];

    float gpioVoltage[BMS_TEMP_CHANNEL_COUNT];

    bool valid;

} BMS_TemperatureData_t;

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

void BMS_App_Init(void);

bool BMS_App_UpdateVoltages(void);

bool BMS_App_UpdateTemperatureInputs(void);

void BMS_App_CheckVoltageFaults(void);

const BMS_VoltageData_t *BMS_App_GetVoltageData(void);

const BMS_TemperatureData_t *BMS_App_GetTemperatureData(void);

const BMS_FaultData_t *BMS_App_GetFaultData(void);

#endif
