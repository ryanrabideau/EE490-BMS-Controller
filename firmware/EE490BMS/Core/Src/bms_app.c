#include "bms_app.h"

#include "L9963E_utils.h"

#include <stddef.h>
#include <stdio.h>

#define BMS_GPIO_VOLTAGE_LSB_V 0.000089f

/*
 * L9963E current ADC conversion period in Normal state.
 * Datasheet typical value: 328.25 microseconds.
 */
#define BMS_CURRENT_ADC_PERIOD_S 0.00032825f

/*
 * Current polarity configuration.
 *
 * Set to +1.0f if positive accumulated charge
 * means the battery is charging.
 *
 * Set to -1.0f if positive accumulated charge
 * means the battery is discharging.
 *
 * Verify this on hardware before relying on SoC.
 */
#define BMS_SOC_CURRENT_DIRECTION 1.0f

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
static BMS_CoulombData_t coulombData;
static BMS_SocData_t socData;
static BMS_FaultData_t faultData;

//Tracks whether the L9963E current conversion chain has already been successfully enabled.
static bool currentSenseInitialized = false;


/* Initialize the higher-level BMS application state.
 *
 * This clears validity flags and prepares the voltage,
 * current, Coulomb-counting, SoC, and fault structures. */
void BMS_App_Init(void)
{
    voltageData.valid = false;
    temperatureData.valid = false;
    currentData.valid = false;
    currentData.rawCode = 0;
    currentData.senseVoltage = 0.0f;
    currentData.packCurrent = 0.0f;
    coulombData.sampleCount = 0U;
    coulombData.accumulatorCode = 0;
    coulombData.deltaChargeAh = 0.0f;
    coulombData.accumulatedChargeAh = 0.0f;
    coulombData.overflow = false;
    coulombData.valid = false;
    socData.socPercent = 0.0f;
    socData.referenceSocPercent = 0.0f;
    socData.referenceSet = false;
    socData.valid = false;
    currentSenseInitialized = false;
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
     *
     * If the L9963E communication fails, conversion times
     * out, or any requested measurement is unavailable,
     * do not process the previously stored measurement set
     * as though it were new data.
     */
    if (!L9963E_utils_read_cells(0))
    {
        voltageData.valid = false;
        BMS_App_CheckVoltageFaults();

        return false;
    }

    const uint16_t *rawCells = L9963E_utils_get_cells(&cellCount);

    if ((rawCells == NULL) || (cellCount != BMS_CELL_COUNT))
    {
        voltageData.valid = false;
        BMS_App_CheckVoltageFaults();

        return false;
    }

    voltageData.packVoltage = 0.0f;

    for (uint8_t i = 0U; i < BMS_CELL_COUNT; i++)
    {
        voltageData.cellVoltage[i] = L9963E_utils_get_cell_mv(i) / 1000.0f;
        voltageData.packVoltage += voltageData.cellVoltage[i];
    }

    voltageData.minCellVoltage = voltageData.cellVoltage[0];
    voltageData.maxCellVoltage = voltageData.cellVoltage[0];

    voltageData.minCellIndex = 0U;
    voltageData.maxCellIndex = 0U;

    for (uint8_t i = 1U; i < BMS_CELL_COUNT; i++)
    {
        if (voltageData.cellVoltage[i] < voltageData.minCellVoltage)
        {
            voltageData.minCellVoltage = voltageData.cellVoltage[i];
            voltageData.minCellIndex = i;
        }

        if (voltageData.cellVoltage[i] > voltageData.maxCellVoltage)
        {
            voltageData.maxCellVoltage = voltageData.cellVoltage[i];
            voltageData.maxCellIndex = i;
        }
    }

    voltageData.deltaVoltage = voltageData.maxCellVoltage - voltageData.minCellVoltage;
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
     *
     * Do not process previously stored GPIO values if
     * the latest L9963E acquisition did not complete
     * successfully.
     */
    if (!L9963E_utils_read_cells(1))
    {
        temperatureData.valid = false;

        return false;
    }

    const uint16_t *rawGpios = L9963E_utils_get_gpios(&gpioCount);

    if ((rawGpios == NULL) || (gpioCount != BMS_TEMP_CHANNEL_COUNT))
    {
        temperatureData.valid = false;

        return false;
    }

    for (uint8_t i = 0U; i < BMS_TEMP_CHANNEL_COUNT; i++)
    {
        temperatureData.raw[i] = rawGpios[i];

        temperatureData.gpioVoltage[i] = ((float)rawGpios[i]) * BMS_GPIO_VOLTAGE_LSB_V;
    }

    temperatureData.valid = true;

    return true;
}

/*
 * Initialize the L9963E current-sense / Coulomb-counting
 * path once at application startup.
 *
 * The initial 0x7B read clears any stale Coulomb
 * accumulator and sample-count data. That data is
 * intentionally discarded.
 *
 * After the counter is cleared, current sensing and
 * Coulomb counting are enabled.
 */
static bool BMS_App_InitializeCurrentSense(void)
{
    L9963E_CoulombData_t discardData;

    if (currentSenseInitialized) return true;

    //Clear any old Coulomb-counter contents before enabling a new measurement interval.
    if (!L9963E_utils_read_coulomb_counter(&discardData)) return false;
    if (!L9963E_utils_enable_current_sense()) return false;

    currentSenseInitialized = true;

    return true;
}

bool BMS_App_UpdateCurrent(void)
{
    int32_t rawCurrent = 0;

    //Initialize the current-sense path the first time current measurement is requested.
    if (!BMS_App_InitializeCurrentSense())
    {
        currentData.valid = false;
        return false;
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
    currentData.senseVoltage = ((float)rawCurrent) * L9963_CURRENT_LSB_V;

    /*
     * Ohm's law:
     *
     * I = Vshunt / Rshunt
     *
     * BMS_CURRENT_SHUNT_OHMS is currently a temporary
     * configuration value and must be updated when the
     * team's final shunt resistor is selected.
     */
    currentData.packCurrent = currentData.senseVoltage / BMS_CURRENT_SHUNT_OHMS;

    /*
     * Current polarity depends on the physical orientation
     * of ISENSEP, ISENSEM, and the shunt resistor.
     * Do not assume positive means charge or discharge until
     * the team's schematic/hardware orientation is confirmed.
     */
    currentData.valid = true;

    return true;
}

bool BMS_App_UpdateCoulombCount(void)
{
    L9963E_CoulombData_t rawCoulombData;

    /*
     * The current conversion chain must be enabled
     * before the Coulomb Counter can accumulate data.
     */
    if (!BMS_App_InitializeCurrentSense())
    {
        coulombData.valid = false;
        return false;
    }

    /*
     * Read the L9963E Coulomb Counter using burst
     * command 0x7B.
     *
     * A successful read also resets the L9963E's
     * internal accumulator and sample counter for
     * the next interval.
     */
    if (!L9963E_utils_read_coulomb_counter(&rawCoulombData))
    {
        coulombData.valid = false;
        return false;
    }

    coulombData.sampleCount = rawCoulombData.sampleCount;
    coulombData.accumulatorCode = rawCoulombData.accumulatorCode;
    coulombData.overflow = (rawCoulombData.overflow != 0U);

    /*
     * If the hardware reports an accumulator or
     * sample-counter overflow, the charge result
     * cannot be considered reliable.
     */
    if (coulombData.overflow)
    {
        coulombData.valid = false;
        return false;
    }

    /*
     * The L9963E Coulomb accumulator contains the
     * signed sum of the current ADC samples.
     *
     * Convert accumulated ADC counts into charge:
     *
     * delta Q =
     * accumulator
     * x ADC voltage resolution
     * x current ADC sample period
     * / shunt resistance
     *
     * This first gives ampere-seconds (coulombs).
     */
    float deltaChargeAs = ((float)coulombData.accumulatorCode) * L9963_CURRENT_LSB_V *
        BMS_CURRENT_ADC_PERIOD_S / BMS_CURRENT_SHUNT_OHMS;

    /*
     * 1 ampere-hour = 3600 ampere-seconds.
     */
    coulombData.deltaChargeAh = deltaChargeAs / 3600.0f;

    /*
     * Keep a running charge-change total.
     */
    coulombData.accumulatedChargeAh += coulombData.deltaChargeAh;

    coulombData.valid = true;

    return true;
}

/*
 * Temporary starting SoC reference.
 *
 * Only establish the reference after the AFE has
 * initialized successfully. The 80 percent value is
 * still only a software test reference and is NOT a
 * measured battery state of charge.
 */
bool BMS_App_SetSocReference(float initialSocPercent)
{
    if ((initialSocPercent < 0.0f) || (initialSocPercent > 100.0f))
    {
        socData.referenceSet = false;
        socData.valid = false;

        return false;
    }

    /*
     * Start a new SoC estimate from the supplied
     * reference point.
     *
     * Reset the accumulated charge change so the
     * reference corresponds to this moment.
     */
    coulombData.accumulatedChargeAh = 0.0f;

    socData.referenceSocPercent = initialSocPercent;

    socData.socPercent = initialSocPercent;

    socData.referenceSet = true;
    socData.valid = true;

    return true;
}

bool BMS_App_UpdateSoc(void)
{
    if (!socData.referenceSet)
    {
        socData.valid = false;

        return false;
    }

    if (!coulombData.valid)
    {
        socData.valid = false;

        return false;
    }

    //Convert accumulated charge change into a percentage of total pack capacity.
    float socChangePercent = (coulombData.accumulatedChargeAh / BMS_PACK_CAPACITY_AH) *
        100.0f * BMS_SOC_CURRENT_DIRECTION;

    socData.socPercent = socData.referenceSocPercent + socChangePercent;

    //Clamp the estimate to the physical 0-100 percent range.
    if (socData.socPercent > 100.0f)
    {
        socData.socPercent = 100.0f;
    }
    else if (socData.socPercent < 0.0f)
    {
        socData.socPercent = 0.0f;
    }

    socData.valid = true;

    return true;
}

/*
 * The individual application update functions
 * maintain their own validity flags if a later
 * measurement or communication operation fails.
 */
bool BMS_App_UpdateAll(void)
{
    /*
     * Run each operation independently.
     *
     * Do not combine these calls directly with &&
     * because short-circuit evaluation would prevent
     * later measurements from running after an earlier
     * failure.
     */
    bool voltageOk = BMS_App_UpdateVoltages();
//    bool currentOk = BMS_App_UpdateCurrent();
//    bool coulombOk = BMS_App_UpdateCoulombCount();
//    bool socOk = BMS_App_UpdateSoc();

    /*
     * The overall cycle is considered successful only
     * if every requested subsystem updated successfully.
     *
     * Each subsystem still maintains its own validity
     * flag, so UART telemetry can show partial failures.
     */
//    return voltageOk && currentOk && coulombOk && socOk;
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
        faultData.fault = BMS_VOLTAGE_DATA_INVALID;
        faultData.faultActive = true;

        return;
    }

    //Check every cell for an undervoltage or overvoltage condition.
    for (uint8_t i = 0U; i < BMS_CELL_COUNT; i++)
    {
        if (voltageData.cellVoltage[i] < BMS_CELL_UV_THRESHOLD_V)
        {
            faultData.fault = BMS_CELL_UNDERVOLTAGE;

            faultData.faultCellIndex = i;
            faultData.faultActive = true;
            return;
        }

        if (voltageData.cellVoltage[i] > BMS_CELL_OV_THRESHOLD_V)
        {
            faultData.fault = BMS_CELL_OVERVOLTAGE;

            faultData.faultCellIndex = i;
            faultData.faultActive = true;
            return;
        }
    }
}

/*
 * Convert the latest stored BMS measurements into one
 * human-readable line suitable for UART transmission.
 *
 * Integer formatting is used instead of printf "%f" so
 * the project does not require newlib-nano float printf
 * support.
 */
int BMS_App_FormatTelemetry( char *buffer, unsigned int bufferSize)
{
    if ((buffer == NULL) || (bufferSize == 0U)) return -1;

    //Voltages to ints
    uint32_t packMv = (uint32_t)((voltageData.packVoltage * 1000.0f) + 0.5f);
    uint32_t cellMv[BMS_CELL_COUNT];
    for (uint8_t i = 0U; i < BMS_CELL_COUNT; i++)
    {
        cellMv[i] = (uint32_t)((voltageData.cellVoltage[i] * 1000.0f) + 0.5f);
    }

    //Currents to ints
    int32_t currentMa = (int32_t)(currentData.packCurrent * 1000.0f);
    char currentSign;
    uint32_t currentMagnitudeMa;

    if (currentMa < 0)
    {
        currentSign = '-';
        currentMagnitudeMa = (uint32_t)(-1*currentMa);
    }
    else
    {
    	currentSign = '+';
        currentMagnitudeMa = (uint32_t)currentMa;
    }

    //SOC to int
    uint32_t socTenths = (uint32_t)((socData.socPercent * 10.0f) + 0.5f);

    return snprintf(
        buffer,
        bufferSize,

        "PACK: %lu.%03lu V | "
        "CELLS: "
        "%lu.%03lu "
        "%lu.%03lu "
        "%lu.%03lu "
        "%lu.%03lu "
        "%lu.%03lu "
        "%lu.%03lu "
        "%lu.%03lu V | "
        "CURRENT: %c%lu.%03lu A | "
        "SOC: %lu.%01lu %% | "
        "VALID[V:%u I:%u SOC:%u]\r\n",

        (unsigned long)(packMv / 1000U),
        (unsigned long)(packMv % 1000U),

        (unsigned long)(cellMv[0] / 1000U),
        (unsigned long)(cellMv[0] % 1000U),

        (unsigned long)(cellMv[1] / 1000U),
        (unsigned long)(cellMv[1] % 1000U),

        (unsigned long)(cellMv[2] / 1000U),
        (unsigned long)(cellMv[2] % 1000U),

        (unsigned long)(cellMv[3] / 1000U),
        (unsigned long)(cellMv[3] % 1000U),

        (unsigned long)(cellMv[4] / 1000U),
        (unsigned long)(cellMv[4] % 1000U),

        (unsigned long)(cellMv[5] / 1000U),
        (unsigned long)(cellMv[5] % 1000U),

        (unsigned long)(cellMv[6] / 1000U),
        (unsigned long)(cellMv[6] % 1000U),

        currentSign,

        (unsigned long)(currentMagnitudeMa / 1000U),
        (unsigned long)(currentMagnitudeMa % 1000U),

        (unsigned long)(socTenths / 10U),
        (unsigned long)(socTenths % 10U),

        voltageData.valid ? 1U : 0U,
        currentData.valid ? 1U : 0U,
        socData.valid ? 1U : 0U);
}

/* ===================== Data Access ===================== */
const BMS_VoltageData_t *BMS_App_GetVoltageData(void) {return &voltageData;}
const BMS_TemperatureData_t *BMS_App_GetTemperatureData(void) {return &temperatureData;}
const BMS_CurrentData_t *BMS_App_GetCurrentData(void) {return &currentData;}
const BMS_CoulombData_t *BMS_App_GetCoulombData(void) {return &coulombData;}
const BMS_SocData_t *BMS_App_GetSocData(void) {return &socData;}
const BMS_FaultData_t *BMS_App_GetFaultData(void) {return &faultData;}
