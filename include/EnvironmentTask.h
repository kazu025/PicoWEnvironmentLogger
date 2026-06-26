#ifndef ENVIRONMENT_TASK_H
#define ENVIRONMENT_TASK_H
/* ----------------------------------------- */
#include <stdint.h>
#include "BME280.h"
#include "EventLogger.h"

struct EnvironmentTaskContext{
    BME280* bme280;
    EventLogger* logger;
    uint32_t period_ms;
};
void environment_task(void* param);
bool getLatestBme280Values(BME280Values& values, uint32_t& timestamp_ms);
/* ----------------------------------------- */
#endif //　ENVIRONMENT_TASK_H

