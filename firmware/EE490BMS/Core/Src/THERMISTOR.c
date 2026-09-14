#include "thermistor.h"
#include <math.h>

#define ADC_MAX     4095.0f
#define VREF        3.3f
#define R_FIXED     10000.0f
#define R0          10000.0f
#define T0          298.15f
#define BETA        3435.0f

float Thermistor_ReadF(ADC_HandleTypeDef *hadc, uint32_t timeout)
{
    HAL_ADC_PollForConversion(hadc, timeout);
    uint32_t raw = HAL_ADC_GetValue(hadc);

    float vadc = (raw / ADC_MAX) * VREF;

    // Guard against divide-by-zero / log(0) at the rails
    if (vadc <= 0.0f)      vadc = 0.0001f;
    if (vadc >= VREF)      vadc = VREF - 0.0001f;

    float r_therm = R_FIXED * (vadc / (VREF - vadc));

    float tempK = 1.0f / ( (1.0f / T0) + (1.0f / BETA) * logf(r_therm / R0) );
    float tempF = (tempK - 273.15f) * 9.0f / 5.0f + 32.0f;

    return tempF;
}
