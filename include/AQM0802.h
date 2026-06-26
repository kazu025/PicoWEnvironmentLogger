#pragma once

#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "FreeRTOS.h"
#include "semphr.h"

class AQM0802 {
public:
    static constexpr uint8_t DEFAULT_ADDR = 0x3E;

    AQM0802(
        i2c_inst_t* i2c_port,
        uint8_t address = DEFAULT_ADDR,
        SemaphoreHandle_t i2c_mutex = nullptr
    );

    bool init();

    bool command(uint8_t cmd);
    bool data(uint8_t value);

    bool clear();
    bool home();
    bool setCursor(uint8_t row, uint8_t col);
    bool print(const char* str);
    bool printLine(uint8_t row, const char* str);
    bool newline();

private:
    static constexpr uint8_t AQM0802_CMD  = 0x00;
    static constexpr uint8_t AQM0802_DATA = 0x40;

    static constexpr uint8_t CLEAR = 0x01;
    static constexpr uint8_t HOME  = 0x02;
    static constexpr uint8_t SET_DDRAM_ADDR = 0x80;

    static constexpr uint8_t CONTRAST = 0x20;
    static constexpr uint32_t I2C_TIMEOUT_US = 10000;

    i2c_inst_t* i2c_port_;
    uint8_t address_;
    SemaphoreHandle_t i2c_mutex_;

    bool write(uint8_t control, uint8_t value);
    void delayMs(uint32_t ms);

    bool lockI2c();
    void unlockI2c();
};
