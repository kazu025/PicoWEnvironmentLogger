#include "BME280.h"

#include <cstdio>
#include "pico/stdlib.h"
#
BME280::BME280(i2c_inst_t* i2c_port, uint8_t address, SemaphoreHandle_t i2c_mutex)
    : i2c_port_(i2c_port), address_(address), i2c_mutex_(i2c_mutex)
{}
bool BME280::lockI2c()
{
    if(i2c_mutex_ == nullptr){
        return true;
    }

    return xSemaphoreTake(i2c_mutex_, pdMS_TO_TICKS(1000)) == pdTRUE;
}

void BME280::unlockI2c()
{
    if(i2c_mutex_ != nullptr){
        xSemaphoreGive(i2c_mutex_);
    }
}

uint16_t BME280::u16_le(const uint8_t* p){
    return static_cast<uint16_t>(p[0]) | static_cast<uint16_t>(p[1] << 8);
}

int16_t BME280::s16_le(const uint8_t* p){
    return static_cast<int16_t>(u16_le(p));
}

int16_t BME280::signExtend12(uint16_t value){
    value &= 0x0FFF;
    if(value & 0x0800){
        value |= 0xF000;
    }
    return static_cast<int16_t>(value);
}

bool BME280::readRegister8(uint8_t reg, uint8_t& value){
    return readRegisters(reg, &value, 1);
}

bool BME280::writeRegister8(uint8_t reg, uint8_t value){
    uint8_t buffer[2] = {reg, value};

    if(!lockI2c()){
        printf("!!! BME280::readRegister8 shared_io_mutex timeout\n");
        return false;
    }
    int ret = i2c_write_timeout_us(i2c_port_, address_, buffer, sizeof(buffer), false, I2C_TIMEOUT_US);
    unlockI2c();

    if(ret != static_cast<int>(sizeof(buffer))){
        printf("!!! BME280::writeRegister8 failed reg=0x%02X value=0x%02X ret=%d\n", reg, value, ret);
        return false;
    }
    return true;
}

bool BME280::readRegisters(uint8_t start_reg, uint8_t* buffer, size_t length){
    if(buffer == nullptr || length == 0){
        return false;
    }

    if(!lockI2c()){
        printf("!!! BME280::readRegisters shared_io_mutex timeout\n");
        return false;
    }
    int ret = i2c_write_timeout_us(i2c_port_, address_, &start_reg, 1, true, I2C_TIMEOUT_US);
    if(ret != 1){
        unlockI2c();
//        printf("!!! BME280::readRegisters write failed reg=0x%02X ret=%d\n", start_reg, ret);
        return false;
    }
    ret = i2c_read_timeout_us(i2c_port_, address_, buffer, length, false, I2C_TIMEOUT_US);
    unlockI2c();
    
    if(ret != static_cast<int>(length)){
        printf("!!! BME280::readRegisters read failed reg=0x%02X len=%u ret=%d\n", start_reg, static_cast<unsigned>(length), ret);
        return false;
    }
    return true;
}

bool BME280::readChipId(uint8_t& id)
{
    return readRegister8(REG_ID, id);
}

bool BME280::readCalibration(){
    uint8_t b1[26];
    uint8_t b2[7];

    if(!readRegisters(REG_CALIB00, b1, sizeof(b1))){
        return false;
    }
    if(!readRegisters(REG_CALIB26, b2, sizeof(b2))){
        return false;
    }
    cal_.dig_T1 = u16_le(&b1[0]);
    cal_.dig_T2 = s16_le(&b1[2]);
    cal_.dig_T3 = s16_le(&b1[4]);

    cal_.dig_P1 = u16_le(&b1[6]);
    cal_.dig_P2 = s16_le(&b1[8]);
    cal_.dig_P3 = s16_le(&b1[10]);
    cal_.dig_P4 = s16_le(&b1[12]);
    cal_.dig_P5 = s16_le(&b1[14]);
    cal_.dig_P6 = s16_le(&b1[16]);
    cal_.dig_P7 = s16_le(&b1[18]);
    cal_.dig_P8 = s16_le(&b1[20]);
    cal_.dig_P9 = s16_le(&b1[22]);

    cal_.dig_H1 = b1[25];

    cal_.dig_H2 = s16_le(&b2[0]);
    cal_.dig_H3 = b2[2];    

    uint16_t h4 = static_cast<uint16_t>((static_cast<uint16_t>(b2[3]) << 4) | (b2[4] & 0x0F));

    uint16_t h5 = static_cast<uint16_t>((static_cast<uint16_t>(b2[5]) << 4) | (b2[4] >> 4));

    cal_.dig_H4 = signExtend12(h4);
    cal_.dig_H5 = signExtend12(h5);
    cal_.dig_H6 = static_cast<int8_t>(b2[6]);

    cal_.t_fine = 0;

    return true;
}

bool BME280::configureNormalMode(){
    if(!writeRegister8(REG_RESET, RESET_VALUE)){
        return false;
    }
    sleep_ms(10);
    // standby=1000ms, filter=16, spi3w=0
    if(!writeRegister8(REG_CONFIG, static_cast<uint8_t>((0x05 << 5) | (0x04 << 2)))){
        return false;
    }
    //humidity oversampling x1
    //BME280では ctrl_humを書いた後 ctrl_measを書く
    if(!writeRegister8(REG_CTRL_HUM, 0x01)){
        return false;
    }
    //temperature x1, presure x1, normal mode
    if(!writeRegister8(REG_CTRL_MEAS, static_cast<uint8_t>((0x01<<5)|(0x01<<2) | 0x03))){
        return false;
    }
    // 初回計測完了待ち
    sleep_ms(1000);
    return true;
}
bool BME280::init(){
    uint8_t id = 0;
    if(!readChipId(id)){
        printf("!!! BME280::init readChipId failed\n");
        return false;
    }
    
    printf("BME280 id=0x%02X expect=0x%02X\n", id, CHIP_ID);
    
    if(id != CHIP_ID){
        printf("!!! BME280::init invalid chip id\n");
        return false;
    }

    if(!readCalibration()){
        printf("!!! BME280::init readCalibration failed\n");
        return false;
    }

    if(!configureNormalMode()){
        printf("!!! BME280::init configureNormalMode failed\n");
        return false;
    }

    printf("BME280::init OK\n");
    return true;
}

bool BME280::readRaw(int32_t& adc_T, int32_t& adc_P, int32_t& adc_H)
{
    uint8_t raw[8];

    if(!readRegisters(REG_DATA, raw, sizeof(raw))){
        return false;
    }

    adc_P = static_cast<int32_t>(
        (static_cast<uint32_t>(raw[0]) << 12) |
        (static_cast<uint32_t>(raw[1]) << 4)  |
        (static_cast<uint32_t>(raw[2]) >> 4)
    );

    adc_T = static_cast<int32_t>(
        (static_cast<uint32_t>(raw[3]) << 12) |
        (static_cast<uint32_t>(raw[4]) << 4)  |
        (static_cast<uint32_t>(raw[5]) >> 4)
    );

    adc_H = static_cast<int32_t>(
        (static_cast<uint32_t>(raw[6]) << 8) |
        static_cast<uint32_t>(raw[7])
    );

    return true;
}

int32_t BME280::compensateTemperature01C(int32_t adc_T){
    int32_t var1 =
        ((((adc_T >> 3) - (static_cast<int32_t>(cal_.dig_T1) << 1))) * static_cast<int32_t>(cal_.dig_T2)) >> 11;
    int32_t var2 =
        (((((adc_T >> 4) - static_cast<int32_t>(cal_.dig_T1)) *
           ((adc_T >> 4) - static_cast<int32_t>(cal_.dig_T1))) >> 12) *
         static_cast<int32_t>(cal_.dig_T3)) >> 14;

    cal_.t_fine = var1 + var2;

    return (cal_.t_fine * 5 + 128) >> 8;

}


uint32_t BME280::compensatePressurePa(int32_t adc_P) const
{
    int64_t var1;
    int64_t var2;
    int64_t p;

    var1 = static_cast<int64_t>(cal_.t_fine) - 128000;
    var2 = var1 * var1 * static_cast<int64_t>(cal_.dig_P6);
    var2 = var2 + ((var1 * static_cast<int64_t>(cal_.dig_P5)) << 17);
    var2 = var2 + (static_cast<int64_t>(cal_.dig_P4) << 35);

    var1 =
        ((var1 * var1 * static_cast<int64_t>(cal_.dig_P3)) >> 8) +
        ((var1 * static_cast<int64_t>(cal_.dig_P2)) << 12);

    var1 =
        (((static_cast<int64_t>(1) << 47) + var1) *
         static_cast<int64_t>(cal_.dig_P1)) >> 33;

    if(var1 == 0){
        return 0;
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;

    var1 =
        (static_cast<int64_t>(cal_.dig_P9) *
         (p >> 13) *
         (p >> 13)) >> 25;

    var2 =
        (static_cast<int64_t>(cal_.dig_P8) * p) >> 19;

    p =
        ((p + var1 + var2) >> 8) +
        (static_cast<int64_t>(cal_.dig_P7) << 4);

    if(p < 0){
        return 0;
    }

    return static_cast<uint32_t>(p >> 8);
}

uint32_t BME280::compensateHumidity1024(int32_t adc_H) const
{
    int32_t v_x1_u32r;

    v_x1_u32r = cal_.t_fine - 76800;

    v_x1_u32r =
        (((((adc_H << 14) -
            (static_cast<int32_t>(cal_.dig_H4) << 20) -
            (static_cast<int32_t>(cal_.dig_H5) * v_x1_u32r)) +
           16384) >> 15) *
         (((((((v_x1_u32r * static_cast<int32_t>(cal_.dig_H6)) >> 10) *
              (((v_x1_u32r * static_cast<int32_t>(cal_.dig_H3)) >> 11) + 32768)) >> 10) +
            2097152) *
           static_cast<int32_t>(cal_.dig_H2) +
           8192) >> 14));

    v_x1_u32r =
        v_x1_u32r -
        (((((v_x1_u32r >> 15) *
            (v_x1_u32r >> 15)) >> 7) *
          static_cast<int32_t>(cal_.dig_H1)) >> 4);

    if(v_x1_u32r < 0){
        v_x1_u32r = 0;
    }

    if(v_x1_u32r > 419430400){
        v_x1_u32r = 419430400;
    }

    return static_cast<uint32_t>(v_x1_u32r >> 12);
}

bool BME280::read(BME280Values& out)
{
    int32_t adc_T = 0;
    int32_t adc_P = 0;
    int32_t adc_H = 0;

    if(!readRaw(adc_T, adc_P, adc_H)){
        return false;
    }

    int32_t temperature_01c = compensateTemperature01C(adc_T);
    uint32_t pressure_pa = compensatePressurePa(adc_P);
    uint32_t humidity_1024 = compensateHumidity1024(adc_H);

    out.temperature_c = static_cast<float>(temperature_01c) / 100.0f;
    out.pressure_hpa = static_cast<float>(pressure_pa) / 100.0f;
    out.humidity_rh = static_cast<float>(humidity_1024) / 1024.0f;

    return true;
}
