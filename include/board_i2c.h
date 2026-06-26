#ifndef BOARD_I2C_H
#define BOARD_I2C_H
#include "hardware/i2c.h"

#define I2C_PORT0 i2c0
#define I2C_PORT1 i2c1
#define I2C_SDA_PIN PICO_DEFAULT_I2C_SDA_PIN // 4
#define I2C_SCL_PIN PICO_DEFAULT_I2C_SCL_PIN // 5
#define I2C_BAUDRATE (50 * 1000)
//#define I2C_BAUDRATE (100 * 1000)

void board_i2c_init(i2c_inst_t* i2c, uint sda = I2C_SDA_PIN, uint scl = I2C_SCL_PIN, uint32_t baudrate = I2C_BAUDRATE);
void scan_i2c_bus(i2c_inst_t* i2c);
#endif //
