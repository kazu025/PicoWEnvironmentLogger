#ifndef ADC_LOGGER_TASK_H
#define ADC_LOGGER_TASK_H
/* ----------------------------------------------------------- */
#include <stdint.h>
#include "EventLogger.h"
struct AdcLatestValue {
    uint16_t raw;
    uint16_t avg;
    float   voltage;
    uint32_t timestamp_ms;
    bool valid;
};
void adc_task(void* param);
AdcLatestValue getAdcLatestValue();
/* ------------------------------------------------------------ */
#endif // ADC_LOGGER_TASK_H
