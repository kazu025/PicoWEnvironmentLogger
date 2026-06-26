#include "AEADT7410.h"
#include <cstdio>

AEADT7410::AEADT7410(
    i2c_inst_t* i2c_port,
    uint8_t address,
    SemaphoreHandle_t i2c_mutex
)
    : i2c_port_(i2c_port),
      address_(address),
      i2c_mutex_(i2c_mutex),
      initialized_(false)
{
}

bool AEADT7410::init()
{
    // ADT7410を16bit分解能モードに設定する。
    // I2Cバスの初期化とGPIO設定はmain.cppで一度だけ行う。
    initialized_ = writeReg(CONFIG_REG, 0x80);
    return initialized_;
}

bool AEADT7410::writeReg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};

    if(!lockI2c()){
        printf("!!! AEADT7410::writeReg i2c mutex timeout\n");
        return false;
    }

    int ret = i2c_write_timeout_us(
        i2c_port_,
        address_,
        buf,
        2,
        false,
        I2C_TIMEOUT_US
    );

    unlockI2c();
    if(ret != 2){
        printf("!!! AEADT7410::writeReg failed ret=%d\n", ret);
        return false;
    }
    return true;
}

bool AEADT7410::readTempRaw(uint8_t& msb, uint8_t& lsb)
{
    uint8_t reg = TEMP_REG;
    uint8_t buf[2] = {};

    if(!lockI2c()){
        printf("!!! AEADT7410::readTempRaw i2c mutex timeout\n");
        return false;
    }

    int ret = i2c_write_timeout_us(i2c_port_, address_, &reg, 1, true, I2C_TIMEOUT_US);

    if(ret != 1){
        unlockI2c();
        printf("!!! AEADT7410::readTempRaw write failed ret=%d\n", ret);
        return false;
    }

    ret = i2c_read_timeout_us(i2c_port_, address_, buf, 2, false, I2C_TIMEOUT_US);

    unlockI2c();

    if(ret != 2){
        printf("!!! AEADT7410::readTempRaw read failed ret=%d\n", ret);
        return false;
    }

    msb = buf[0];
    lsb = buf[1];

    return true;
}

float AEADT7410::convertTemp(uint8_t msb, uint8_t lsb)
{
    uint16_t u16 =
        (static_cast<uint16_t>(msb) << 8) |
        static_cast<uint16_t>(lsb);

    int16_t raw = static_cast<int16_t>(u16);

    // 16bit mode: 1 LSB = 1 / 128 ℃
    return static_cast<float>(raw) / 128.0f;
}

bool AEADT7410::readTemperature(float& temperature_c)
{
    uint8_t msb = 0;
    uint8_t lsb = 0;

    if(!readTempRaw(msb, lsb)){
        return false;
    }

    temperature_c = convertTemp(msb, lsb);
    return true;
}

bool AEADT7410::lockI2c()
{
    if(i2c_mutex_ == nullptr){
        return true;
    }

    return xSemaphoreTake(i2c_mutex_, pdMS_TO_TICKS(1000)) == pdTRUE;
}

void AEADT7410::unlockI2c()
{
    if(i2c_mutex_ != nullptr){
        xSemaphoreGive(i2c_mutex_);
    }
}