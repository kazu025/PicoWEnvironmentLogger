#pragma once
#include <stdint.h>
enum class DisplayMode : uint8_t {
    Temperature = 0,
    AdcVoltage,
    LogStatus,
    I2cInfo,
    BmeTemperature,
    BmeHumidity,
    BmePressure,
    WiFiIp,
    Max
};
