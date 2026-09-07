#ifndef L9963E_UTILS_H
#define L9963E_UTILS_H

#include <inttypes.h>

#define L9963_VRES 0.000089f

#define CELLS_N 7
#define GPIOS_N 7

/*
 * Keep the current hardware-test cell configuration unchanged
 * until the team's physical cell setup is confirmed.
 */
#define ENABLED_CELLS \
    (L9963E_CELL1 | L9963E_CELL2 | L9963E_CELL13 | L9963E_CELL14)

/*
 * L9963E current ADC resolution:
 * 1.33 microvolts per signed ADC count.
 */
#define L9963_CURRENT_LSB_V 0.00000133f

void L9963E_utils_init(void);

void L9963E_utils_read_cells(uint8_t read_gpio);

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

#endif /* L9963E_UTILS_H */
