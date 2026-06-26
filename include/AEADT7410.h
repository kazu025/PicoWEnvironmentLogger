#pragma once

#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "FreeRTOS.h"
#include "semphr.h"

class AEADT7410 {
public:
    static constexpr uint8_t DEFAULT_ADDR = 0x48;

    AEADT7410(
        i2c_inst_t* i2c_port,
        uint8_t address = DEFAULT_ADDR,
        SemaphoreHandle_t i2c_mutex = nullptr
    );

    bool init();
    bool readTemperature(float& temperature_c);

    bool isInitialized() const { return initialized_; }
    void clearInitialized() { initialized_ = false; }

private:
    static constexpr uint8_t TEMP_REG   = 0x00;
    static constexpr uint8_t CONFIG_REG = 0x03;
    static constexpr uint32_t I2C_TIMEOUT_US = 10000;

    i2c_inst_t* i2c_port_;
    uint8_t address_;
    SemaphoreHandle_t i2c_mutex_;
    bool initialized_;

    bool writeReg(uint8_t reg, uint8_t value);
    bool readTempRaw(uint8_t& msb, uint8_t& lsb);
    static float convertTemp(uint8_t msb, uint8_t lsb);

    bool lockI2c();
    void unlockI2c();
};
