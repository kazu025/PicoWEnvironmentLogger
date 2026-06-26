#include "board_i2c.h"
#include "hardware/gpio.h"
#include <cstdio>
void board_i2c_init(i2c_inst_t* i2c, uint sda, uint scl, uint32_t baudrate)
{
    i2c_init(i2c, baudrate);

    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);

    gpio_pull_up(sda);
    gpio_pull_up(scl);
    printf("I2C init: i2c0 SDA=%u SCL=%u baud=%lu\n",
           sda,
           scl,
           static_cast<unsigned long>(baudrate));
}

void scan_i2c_bus(i2c_inst_t* i2c)
{
    printf("I2C scan start\n");

    for(uint8_t addr = 0x08; addr < 0x78; addr++){
        uint8_t dummy = 0x00;

        int ret = i2c_write_timeout_us(
            i2c,
            addr,
            &dummy,
            1,
            false,
            3000
        );

        if(ret >= 0){
            printf("I2C device found: 0x%02X ret=%d\n", addr, ret);
        }
    }

    printf("I2C scan done\n");
}