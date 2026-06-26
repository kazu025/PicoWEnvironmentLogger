#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "AdcLoggerTask.h"
#include "hardware/adc.h"
#include "utility.h"
#include "CommandTask.h"

static AdcLatestValue g_adc_latest = {
    0,
    0,
    0.0f,
    0,
    false
};
/* ADC タスク */
void adc_task(void *param){
    auto* logger = static_cast<EventLogger*>(param);

    adc_init();
    adc_gpio_init(26);  // GPIO26 = ADC0
    adc_select_input(0);
    MovingAverage<uint16_t, 16> adc_avg;

    TickType_t last_wake = xTaskGetTickCount();

    while(true){
        uint16_t raw = adc_read();
        uint32_t timestamp_ms = to_ms_since_boot(get_absolute_time());

        adc_avg.add(raw);
        uint16_t raw_avg = adc_avg.average();
        float voltage = static_cast<float>(raw_avg) * 3.3f / 4095.0;
        taskENTER_CRITICAL();
        g_adc_latest.raw = raw;
        g_adc_latest.avg = raw_avg;
        g_adc_latest.voltage = voltage;
        g_adc_latest.timestamp_ms = timestamp_ms;
        g_adc_latest.valid = true;
        taskEXIT_CRITICAL();
        if(logger != nullptr && !isLogPaused()){
            logger->logf(
                LogLevel::INFO, "ADC,%lu,%u,%u,%.3f",
                static_cast<unsigned long>(timestamp_ms),
                static_cast<unsigned>(raw),
                static_cast<unsigned>(raw_avg),
                static_cast<double>(voltage));
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));
    }
}
/* ADC値を返す */
AdcLatestValue getAdcLatestValue(){
    AdcLatestValue value;
    taskENTER_CRITICAL();
    value = g_adc_latest;
    taskEXIT_CRITICAL();
    return value;
}



