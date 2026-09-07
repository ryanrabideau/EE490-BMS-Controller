#ifndef BMS_APP_H
#define BMS_APP_H

#include <stdint.h>
#include <stdbool.h>

/* ===================== Configuration ===================== */

#define BMS_CELL_COUNT           7U
#define BMS_TEMP_CHANNEL_COUNT   7U

/*
 * Battery pack configuration.
 *
 * Samsung INR18650-25R:
 * 2.5 Ah nominal capacity per cell.
 *
 * Pack configuration: 2P7S
 * Pack capacity = 2 x 2.5 Ah = 5.0 Ah.
 */
#define BMS_PACK_CAPACITY_AH     5.0f

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

/* ===================== Coulomb Counting Data ===================== */

typedef struct
{
    /*
     * Number of current ADC samples accumulated
     * during the most recent Coulomb-counting interval.
     */
    uint16_t sampleCount;

    /*
     * Signed raw 32-bit accumulated current ADC sum
     * returned by the L9963E.
     */
    int32_t accumulatorCode;

    /*
     * Charge transferred during the most recent
     * Coulomb-counting interval.
     *
     * Positive/negative direction depends on the
     * physical current-sense polarity.
     */
    float deltaChargeAh;

    /*
     * Running accumulated charge change since
     * BMS_App_Init() was called.
     */
    float accumulatedChargeAh;

    bool overflow;

    bool valid;

} BMS_CoulombData_t;

/* ===================== State of Charge Data ===================== */

typedef struct
{
    /*
     * Current estimated state of charge.
     * Valid range: 0.0 to 100.0 percent.
     */
    float socPercent;

    /*
     * SoC value supplied when the estimate
     * was initialized.
     */
    float referenceSocPercent;

    /*
     * True after a valid starting SoC
     * reference has been supplied.
     */
    bool referenceSet;

    /*
     * True when the current SoC estimate
     * can be considered valid.
     */
    bool valid;

} BMS_SocData_t;

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

bool BMS_App_UpdateCoulombCount(void);

bool BMS_App_SetSocReference(float initialSocPercent);
bool BMS_App_UpdateSoc(void);

void BMS_App_CheckVoltageFaults(void);

/* ===================== Data Access ===================== */

const BMS_VoltageData_t *BMS_App_GetVoltageData(void);

const BMS_TemperatureData_t *BMS_App_GetTemperatureData(void);

const BMS_CurrentData_t *BMS_App_GetCurrentData(void);

const BMS_CoulombData_t *BMS_App_GetCoulombData(void);

const BMS_SocData_t *BMS_App_GetSocData(void);

const BMS_FaultData_t *BMS_App_GetFaultData(void);

#endif
