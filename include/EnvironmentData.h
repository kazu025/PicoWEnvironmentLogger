#pragma once

#include <stdint.h>
#include <stdbool.h>

struct EnvironmentData{
    float temperature;
    float humidity;
    float pressure;
    uint32_t tick;
    bool valid;
};

void environment_data_set(float temperature, float humidity, float pressure);
EnvironmentData environment_data_get(void);



