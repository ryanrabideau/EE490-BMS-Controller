#ifndef THERMISTOR_H
#define THERMISTOR_H

#include "stm32f4xx_hal.h"

// Reads one ADC channel/rank and converts it to temperature in Fahrenheit.
// Call this once per rank, in the same order your ranks are configured in CubeMX.
// Does NOT call HAL_ADC_Start()/Stop() - caller manages the conversion sequence.
float Thermistor_ReadF(ADC_HandleTypeDef *hadc, uint32_t timeout);

#endif /* THERMISTOR_H */
