#ifndef L9963E_UTILS_H
#define L9963E_UTILS_H

#include <inttypes.h>


/* ===================== General configuration ===================== */

#define L9963_VRES 0.000089f

#define CELLS_N 4
#define GPIOS_N 0

/*
 * Keep the current hardware-test cell configuration unchanged
 * until the team's physical cell setup is confirmed.
 *
 * NOTE:
 * The higher-level application expects seven cells:
 * CELL1, CELL2, CELL3, CELL4, CELL12, CELL13, CELL14.
 *
 * Michael's present bench configuration only enables:
 * CELL1, CELL2, CELL13, CELL14.
 *
 * This must be confirmed on the physical 7S pack before final
 * hardware integration.
 */
#define ENABLED_CELLS (L9963E_CELL1 | L9963E_CELL2 | L9963E_CELL13 | L9963E_CELL14)

/*
 * L9963E current ADC resolution:
 * 1.33 microvolts per signed ADC count.
 */
#define L9963_CURRENT_LSB_V 0.00000133f

/* ===================== Initialization ===================== */

/*
 * Initialize and configure the L9963E measurement device.
 *
 * Returns:
 *   1 = initialization completed successfully
 *   0 = initialization or communication failure
 */
uint8_t L9963E_utils_init(void);

/* ===================== Voltage / GPIO acquisition ===================== */

/*
 * Start an ADC conversion and acquire all configured cell
 * measurements.
 *
 * If read_gpio is nonzero, GPIO3 through GPIO9 are also read.
 *
 * Returns:
 *   1 = complete measurement set acquired successfully
 *   0 = communication failure, conversion timeout, or data timeout
 *
 * On failure, the previously stored global measurement set is
 * preserved so partially updated data is never published.
 */
uint8_t L9963E_utils_read_cells(uint8_t read_gpio);


/* ===================== Measurement access ===================== */

uint16_t const *L9963E_utils_get_gpios(uint8_t *len);

uint16_t const *L9963E_utils_get_cells(uint8_t *len);

float L9963E_utils_get_cell_mv(uint8_t index);

void L9963E_utils_get_batt_mv(float *v_tot, float *v_sum);


/* ===================== Current sensing ===================== */

/*
 * Enable the L9963E current conversion chain by setting
 * CoulombCounter_en in CSA_GPIO_MSK.
 *
 * Returns 1 on success and 0 on communication failure.
 */
uint8_t L9963E_utils_enable_current_sense(void);


/*
 * Read the continuously updated instantaneous current
 * measurement from Ibattery_calib.
 *
 * The returned value is the signed 18-bit ADC result,
 * sign-extended into a normal int32_t.
 *
 * Returns 1 on success and 0 on communication failure.
 */
uint8_t L9963E_utils_read_current_raw(int32_t *raw_current);


/* ===================== Coulomb counting ===================== */

typedef struct
{
    uint16_t sampleCount;

    /*
     * Signed sum of all current ADC samples collected
     * since the previous 0x7B burst read.
     */
    int32_t accumulatorCode;

    /*
     * Instantaneous current samples included in the
     * 0x7B response.
     */
    int32_t currentSynchRaw;
    int32_t currentCalibRaw;

    /*
     * Set if the L9963E reports accumulator/sample
     * counter overflow.
     */
    uint8_t overflow;

} L9963E_CoulombData_t;


/*
 * Read the L9963E Coulomb Counter using burst command 0x7B.
 *
 * Important:
 * The L9963E resets its internal accumulator and sample
 * counter when this burst is read.
 *
 * Returns 1 on success and 0 on communication failure.
 */
uint8_t L9963E_utils_read_coulomb_counter(L9963E_CoulombData_t *data);


#endif /* L9963E_UTILS_H */
