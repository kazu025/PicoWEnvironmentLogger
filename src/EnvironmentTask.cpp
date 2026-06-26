#include "EnvironmentTask.h"

#include "FreeRTOS.h"
#include "task.h"
#include "CommandTask.h"
#include "FlashBusy.h"
#include <cstdio>
#include "EnvironmentData.h"

static constexpr uint32_t DEFAULT_PERIOD_MS = 5000;
static BME280Values g_latest_bme280_values{};
static uint32_t g_latest_bme280_timestamp_ms = 0;
static bool g_latest_bme280_valid = false;

bool getLatestBme280Values(BME280Values& values, uint32_t& timestamp_ms){
    bool valid = false;
    taskENTER_CRITICAL();
    values = g_latest_bme280_values;
    timestamp_ms = g_latest_bme280_timestamp_ms;
    valid = g_latest_bme280_valid;
    taskEXIT_CRITICAL();

    return valid;
}
static void updateLatestBme280Values(const BME280Values& values, uint32_t timestamp_ms)
{
    taskENTER_CRITICAL();
    g_latest_bme280_values = values;
    g_latest_bme280_timestamp_ms = timestamp_ms;
    g_latest_bme280_valid = true;
    taskEXIT_CRITICAL();
}

static bool readBme280WithRetry(BME280* bme280, BME280Values& values)
{
    constexpr int RETRY_COUNT = 3;

    for(int i = 0; i < RETRY_COUNT; ++i){
        if(bme280->read(values)){
            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return false;
}
void environment_task(void* param){
    auto* ctx = static_cast<EnvironmentTaskContext*>(param);
    if(ctx == nullptr || ctx->bme280 == nullptr){
        printf("!!! environment_task: invalid context\n");
        vTaskDelete(nullptr);
        return;
    }   
    const uint32_t period_ms = (ctx->period_ms == 0) ? DEFAULT_PERIOD_MS : ctx->period_ms;
    printf("Environment task start: period=%lu[ms]\n", static_cast<unsigned long>(period_ms));

    TickType_t last_wake = xTaskGetTickCount();

    while(true){
        if(isFlashMaintenanceBusy()){
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(period_ms));
            continue;
        }
        BME280Values values{};
        uint32_t timestamp_ms =
            static_cast<uint32_t>(to_ms_since_boot(get_absolute_time()));
        if(readBme280WithRetry(ctx->bme280, values)){
            updateLatestBme280Values(values, timestamp_ms);

            if(ctx->logger != nullptr && !isLogPaused()){
                ctx->logger->logf(LogLevel::INFO, "ENV,%lu,%.2f,%.2f,%.2f",
                    static_cast<unsigned long>(timestamp_ms),
                    values.temperature_c,
                    values.humidity_rh,
                    values.pressure_hpa);
            }
            // 共有バッファに保存
            environment_data_set(values.temperature_c, values.humidity_rh, values.pressure_hpa);
        }else{
            printf("!!! environment_task: BME280 read failed\n");
            if(ctx->logger != nullptr && !isLogPaused()){
                ctx->logger->logf(
                LogLevel::INFO,
                "ENV,%lu,ERR",
                static_cast<unsigned long>(timestamp_ms)
               );
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(period_ms));
    }
}