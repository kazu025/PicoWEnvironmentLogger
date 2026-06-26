#ifndef BME280_H
#define BME280_H

#include <stdint.h>

#include "hardware/i2c.h"
#include "FreeRTOS.h"
#include "semphr.h"

struct BME280Values {
    float temperature_c;
    float pressure_hpa;
    float humidity_rh;
};

class BME280 {
public:
    static constexpr uint8_t ADDR_0x76 = 0x76;
    static constexpr uint8_t ADDR_0x77 = 0x77;
    static constexpr uint8_t DEFAULT_ADDR = ADDR_0x76;

    BME280(
        i2c_inst_t* i2c_port,
        uint8_t address,
        SemaphoreHandle_t i2c_mutex
    );

    bool init();
    bool readChipId(uint8_t& id);
    bool read(BME280Values& out);

private:
    static constexpr uint32_t I2C_TIMEOUT_US = 50000;

    static constexpr uint8_t REG_CALIB00   = 0x88;
    static constexpr uint8_t REG_CALIB26   = 0xE1;
    static constexpr uint8_t REG_ID        = 0xD0;
    static constexpr uint8_t REG_RESET     = 0xE0;
    static constexpr uint8_t REG_STATUS    = 0xF3;
    static constexpr uint8_t REG_CTRL_HUM  = 0xF2;
    static constexpr uint8_t REG_CTRL_MEAS = 0xF4;
    static constexpr uint8_t REG_CONFIG    = 0xF5;
    static constexpr uint8_t REG_DATA      = 0xF7;

    static constexpr uint8_t CHIP_ID       = 0x60;
    static constexpr uint8_t RESET_VALUE   = 0xB6;

    struct Calibration {
        uint16_t dig_T1;
        int16_t  dig_T2;
        int16_t  dig_T3;

        uint16_t dig_P1;
        int16_t  dig_P2;
        int16_t  dig_P3;
        int16_t  dig_P4;
        int16_t  dig_P5;
        int16_t  dig_P6;
        int16_t  dig_P7;
        int16_t  dig_P8;
        int16_t  dig_P9;

        uint8_t  dig_H1;
        int16_t  dig_H2;
        uint8_t  dig_H3;
        int16_t  dig_H4;
        int16_t  dig_H5;
        int8_t   dig_H6;

        int32_t  t_fine;
    };

    i2c_inst_t* i2c_port_;
    uint8_t address_;
    SemaphoreHandle_t i2c_mutex_;
    Calibration cal_{};

    bool lockI2c();
    void unlockI2c();

    bool readRegister8(uint8_t reg, uint8_t& value);
    bool writeRegister8(uint8_t reg, uint8_t value);
    bool readRegisters(uint8_t start_reg, uint8_t* buffer, size_t length);

    bool readCalibration();
    bool configureNormalMode();
    bool readRaw(int32_t& adc_T, int32_t& adc_P, int32_t& adc_H);

    static uint16_t u16_le(const uint8_t* p);
    static int16_t  s16_le(const uint8_t* p);
    static int16_t  signExtend12(uint16_t value);

    int32_t  compensateTemperature01C(int32_t adc_T);
    uint32_t compensatePressurePa(int32_t adc_P) const;
    uint32_t compensateHumidity1024(int32_t adc_H) const;
};

#endif // BME280_H