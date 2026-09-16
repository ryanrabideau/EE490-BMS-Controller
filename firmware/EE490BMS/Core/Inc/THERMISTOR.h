#ifndef THERMISTOR_H
#define THERMISTOR_H

#include "stm32f4xx_hal.h"

// Converts 12-bit ADC of thermistor divider to temperature in degrees F
float Thermistor_ReadF(uint32_t raw);

#endif /* THERMISTOR_H */
