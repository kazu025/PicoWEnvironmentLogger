#include "AQM0802.h"

#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include <cstdio>

AQM0802::AQM0802(
    i2c_inst_t* i2c_port,
    uint8_t address,
    SemaphoreHandle_t i2c_mutex
)
    : i2c_port_(i2c_port),
      address_(address),
      i2c_mutex_(i2c_mutex)
{
}

bool AQM0802::init()
{
    delayMs(100);

    const uint8_t init_cmds1[] = {
        0x38,                              // Function set: 8-bit, 2 line, 5x8 dots
        0x39,                              // Extended instruction set
        0x14,                              // Internal OSC frequency
        static_cast<uint8_t>(0x70 | (CONTRAST & 0x0F)),
        static_cast<uint8_t>(0x56 | ((CONTRAST & 0x30) >> 3)),
        0x6C                               // Follower control
    };

    const uint8_t init_cmds2[] = {
        0x38,                              // Normal instruction set
        0x0C,                              // Display ON, cursor OFF
        CLEAR
    };

    for(size_t i = 0; i < sizeof(init_cmds1) / sizeof(init_cmds1[0]); i++){
        if(!command(init_cmds1[i])){
            printf("!!! AQM0802::init failed CMD1 i=%u\n", static_cast<unsigned>(i));
            return false;
        }
        delayMs(5);
    }

    delayMs(200);

    for(size_t i = 0; i < sizeof(init_cmds2) / sizeof(init_cmds2[0]); i++){
        if(!command(init_cmds2[i])){
            printf("!!! AQM0802::init failed CMD2 i=%u\n", static_cast<unsigned>(i));
            return false;
        }
        delayMs(5);
    }

    return true;
}

bool AQM0802::command(uint8_t cmd)
{
    bool ok = write(AQM0802_CMD, cmd);
    delayMs(2);
    return ok;
}

bool AQM0802::data(uint8_t value)
{
    bool ok = write(AQM0802_DATA, value);
    delayMs(2);
    return ok;
}

bool AQM0802::clear()
{
    bool ok = command(CLEAR);
    delayMs(2);
    return ok;
}

bool AQM0802::home()
{
    bool ok = command(HOME);
    delayMs(2);
    return ok;
}

bool AQM0802::setCursor(uint8_t row, uint8_t col)
{
    uint8_t addr = (row == 0) ? col : static_cast<uint8_t>(0x40 + col);
    return command(static_cast<uint8_t>(SET_DDRAM_ADDR | addr));
}

bool AQM0802::print(const char* str)
{
    if(str == nullptr){
        return false;
    }

    while(*str != '\0'){
        if(!data(static_cast<uint8_t>(*str))){
            return false;
        }
        str++;
    }

    return true;
}

bool AQM0802::printLine(uint8_t row, const char* str)
{
    char buf[9];

    memset(buf, ' ', 8);
    buf[8] = '\0';

    if(str != nullptr){
        size_t len = strlen(str);
        if(len > 8){
            len = 8;
        }
        memcpy(buf, str, len);
    }

    if(!setCursor(row, 0)){
        return false;
    }

    return print(buf);
}

bool AQM0802::newline()
{
    return setCursor(1, 0);
}

bool AQM0802::write(uint8_t control, uint8_t value)
{
    uint8_t buf[2] = {control, value};

    if(!lockI2c()){
        printf("!!! AQM0802::write shared_io_mutex timeout\n");
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
        printf("!!! AQM0802::write failed ret=%d\n", ret);
    }
    return ret == 2;
}

void AQM0802::delayMs(uint32_t ms)
{
    if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING){
        vTaskDelay(pdMS_TO_TICKS(ms));
    }else{
        sleep_ms(ms);
    }
}

bool AQM0802::lockI2c()
{
    if(i2c_mutex_ == nullptr){
        return true;
    }

    return xSemaphoreTake(i2c_mutex_, pdMS_TO_TICKS(1000)) == pdTRUE;
}

void AQM0802::unlockI2c()
{
    if(i2c_mutex_ != nullptr){
        xSemaphoreGive(i2c_mutex_);
    }
}