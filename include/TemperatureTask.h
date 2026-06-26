#pragma once

#include "AEADT7410.h"
#include "AQM0802.h"
#include "EventLogger.h"
#include "FlashLogStorage.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "BME280.h"
struct TemperatureTaskContext {
    AEADT7410* sensor;
    AQM0802* lcd;
    EventLogger* logger;
    FlashLogStorage* storage;
    BME280* bme280;
    QueueHandle_t display_mode_queue;
};

void temperature_task(void* param);
