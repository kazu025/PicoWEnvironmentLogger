#include "EnvironmentData.h"

#include "FreeRTOS.h"
#include "task.h"

static EnvironmentData g_env_data = {
    .temperature = 0.0,
    .humidity = 0.0f,
    .pressure = 0.0f,
    .tick = 0,
    .valid = false
};

void environment_data_set(float temperature, float humidity, float pressure){
    taskENTER_CRITICAL();
    g_env_data.temperature = temperature;
    g_env_data.humidity = humidity;
    g_env_data.pressure = pressure;
    g_env_data.tick = xTaskGetTickCount();
    g_env_data.valid =  true;
    taskEXIT_CRITICAL();
}

EnvironmentData environment_data_get(void){
    EnvironmentData copy;
    taskENTER_CRITICAL();
    copy = g_env_data;
    taskEXIT_CRITICAL();
    return copy;
}


