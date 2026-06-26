#include "TemperatureTask.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "CommandTask.h"
#include "LogTypes.h"
#include "led25.h"
#include "DisplayMode.h"
#include "AdcLoggerTask.h"
#include "EnvironmentTask.h"
#include "FlashBusy.h"
#include "WifiStatus.h"

static constexpr uint32_t TASK_PERIOD_MS = 1000;
static constexpr bool ENABLE_TASK_START_LOG = true;
static constexpr bool ENABLE_STACK_HWM_LOG = true;
static constexpr uint32_t HWM_LOG_INTERVAL_COUNT = 60;
static void log_temperature(EventLogger* logger, uint32_t timestamp_ms, float temperature_c, bool valid) {
    if(logger == nullptr || isLogPaused()){ return; }

    logger->logf(LogLevel::INFO, "TEMP,%lu,%.6f,%s",
        static_cast<unsigned long>(timestamp_ms),
        static_cast<double>(temperature_c),
        valid ? "OK" : "NG"
    );
}
/* ------------------------------------------------------------------------------------------------------ */
static void display_temperature(AQM0802* lcd, bool valid, uint32_t count, float temperature_c) {
    if(lcd == nullptr){ return; }

    char line1[9];
    char line2[9];
    if(valid){
        snprintf(line1, sizeof(line1), "T:%5.2f", static_cast<double>(temperature_c));
        snprintf(line2, sizeof(line2), "OK %04lu", static_cast<unsigned long>(count % 10000));
    }else{
        snprintf(line1, sizeof(line1), "T:--.--");
        snprintf(line2, sizeof(line2), "NG %04lu", static_cast<unsigned long>(count % 10000));
    }
    lcd->printLine(0, line1);
    lcd->printLine(1, line2);
}
/* ------------------------------------------------------------------------------------------------------ */
static bool readAdt7410WithRetry(AEADT7410* sensor, float& temperature_c)
{
    if(sensor == nullptr){
        return false;
    }

    constexpr int RETRY_COUNT = 3;

    for(int i = 0; i < RETRY_COUNT; ++i){

        if(sensor->readTemperature(temperature_c)){
            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return false;
}
/* ------------------------------------------------------------------------------------------------------ */
void temperature_task(void* param)
{
    auto* ctx = static_cast<TemperatureTaskContext*>(param);
    DisplayMode current_mode = DisplayMode::Temperature;

    if(ctx == nullptr || ctx->sensor == nullptr){
        vTaskDelete(nullptr);
        return;
    }
    if(ctx->lcd==nullptr){
        printf("temperature_task: LCD not available\n");
    }
    if(ctx->display_mode_queue == nullptr){
        printf("temperature_task: display_mode_queue not available\n");
    }

    led_init();

    TickType_t last_wake = xTaskGetTickCount();
    uint32_t count = 0;
    bool lcd_initialized = false;

    if(ctx->logger != nullptr && ENABLE_TASK_START_LOG && !isLogPaused()){
        ctx->logger->logf(LogLevel::INFO, "temperature task start");
    }

    if(ctx->lcd != nullptr){
        lcd_initialized = ctx->lcd->init();
        if(lcd_initialized){
            ctx->lcd->printLine(0, "ADT7410");
            ctx->lcd->printLine(1, "START");
        }

        if(ctx->logger != nullptr && !isLogPaused()){
            ctx->logger->logf(LogLevel::INFO, lcd_initialized ? "AQM0802 init OK" : "AQM0802 init NG");
        }
    }

    while(true){
        // Flash メンテナンス中は何もしない
        if(isFlashMaintenanceBusy()){
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        uint32_t timestamp_ms = to_ms_since_boot(get_absolute_time());
        
        /*
         * 初期化されていない場合は、毎周期 init() を再試行する。
         * 起動時にセンサ未接続でも、後から接続/復帰できる。
         */
        if(!ctx->sensor->isInitialized()){
            if(!ctx->sensor->init()){
                log_temperature(ctx->logger, timestamp_ms, 0.0f, false);
                if(lcd_initialized){
                    display_temperature(ctx->lcd, false, count, 0.0f);
                }

                count++;
                led_sw();
                vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_PERIOD_MS));
                continue;
            }

            if(ctx->logger != nullptr && !isLogPaused()){
                ctx->logger->logf(LogLevel::INFO, "AEADT7410 init OK");
            }
        }

        float temperature_c = 0.0f;
        bool valid = readAdt7410WithRetry(ctx->sensor, temperature_c);

        if(!valid){
            /*
             * 読み取り失敗時は、次周期で再初期化を試す。
             */
            ctx->sensor->clearInitialized();
        }

        log_temperature(ctx->logger, timestamp_ms, temperature_c, valid);

        
        if(ENABLE_STACK_HWM_LOG && ctx->logger != nullptr && !isLogPaused() &&
            count > 0 && ((count % HWM_LOG_INTERVAL_COUNT) == 0)){
            
            UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            ctx->logger->logf(LogLevel::INFO, "TEMP_TASK_HWM,%lu",static_cast<unsigned long>(hwm));
        }
        if(ctx->display_mode_queue != nullptr){ 
            DisplayMode new_mode;
            if(xQueueReceive(ctx->display_mode_queue, &new_mode, 0) == pdTRUE){
                current_mode = new_mode;
                if(lcd_initialized && ctx->lcd != nullptr){
                    ctx->lcd->clear();
                }
                if(ctx->logger != nullptr && !isLogPaused()){
                    ctx->logger->logf(LogLevel::INFO, "DisplayMode changed: %d", static_cast<int>(current_mode));
                }
            }
        }
        // --- LCDディスプレイ用　自動表示が切り替わる ---
        //current_mode = static_cast<DisplayMode>(count % 7u);
        // -----------------------------------------------
        if(lcd_initialized && ctx->lcd != nullptr){
            switch(current_mode){
            case DisplayMode::Temperature:
                display_temperature(ctx->lcd, valid, count, temperature_c);
                break;
            case DisplayMode::AdcVoltage:
            {
                AdcLatestValue adc = getAdcLatestValue();
                if(adc.valid){
                    char line[9];

                    snprintf(line, sizeof(line), "%5.3fV", adc.voltage);
                    ctx->lcd->printLine(0, "ADC");
                    ctx->lcd->printLine(1, line);
                } else {
                    ctx->lcd->printLine(0, "ADC");
                    ctx->lcd->printLine(1, "NO DATA");
                }
                break;
            }
            case DisplayMode::LogStatus:
            {
                char line[9];
                uint32_t log_count = 0;
                if(ctx->storage != nullptr) log_count = ctx->storage->getCount();
                snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(log_count));
                ctx->lcd->printLine(0, "LOG CNT");
                ctx->lcd->printLine(1, line);
                break;
            }
            case DisplayMode::BmeTemperature:
            {
                BME280Values values{};
                uint32_t bme_timestamp_ms = 0;
                if(getLatestBme280Values(values, bme_timestamp_ms)){
                    char line[9];
                    snprintf(line,  sizeof(line), "%5.2fC", static_cast<double>(values.temperature_c));
                    ctx->lcd->printLine(0, "BME T");
                    ctx->lcd->printLine(1, line);
                }else{
                    ctx->lcd->printLine(0, "BME T");
                    ctx->lcd->printLine(1, "NO DATA");
                }
                break;
            }
            case DisplayMode::BmeHumidity:
            {
                BME280Values values{};
                uint32_t bme_timestamp_ms = 0;
                if(getLatestBme280Values(values, bme_timestamp_ms)){
                    char line[9];
                    snprintf(line,  sizeof(line), "%5.1f%%", static_cast<double>(values.humidity_rh));
                    ctx->lcd->printLine(0, "BME H");
                    ctx->lcd->printLine(1, line);
                }else{
                    ctx->lcd->printLine(0, "BME H");
                    ctx->lcd->printLine(1, "NO DATA");
                }
                break;
            }
            case DisplayMode::BmePressure:
            {
                BME280Values values{};
                uint32_t bme_timestamp_ms = 0;
                if(getLatestBme280Values(values, bme_timestamp_ms)){
                    char line[9];
                    snprintf(line,  sizeof(line), "%4.0fhPa", static_cast<double>(values.pressure_hpa));
                    ctx->lcd->printLine(0, "BME P");
                    ctx->lcd->printLine(1, line);
                }else{
                    ctx->lcd->printLine(0, "BME P");
                    ctx->lcd->printLine(1, "NO DATA");
                }
                break;
            }
            case DisplayMode::I2cInfo:
                ctx->lcd->printLine(0, "I2C DEV");
                ctx->lcd->printLine(1, "3E 48 76");
                break;
            case DisplayMode::WiFiIp:
            {
                WifiStatus st = wifi_status_get();
                if(st.state == WIFI_STATE_GOT_IP){
                    char line0[9] = {0};
                    char line1[9] = {0};
                    strncpy(line0, st.ip, 8);
                    strncpy(line1, st.ip + 8, 8);
                    ctx->lcd->printLine(0, line0);
                    ctx->lcd->printLine(1, line1);
                }else if(st.state == WIFI_STATE_CONNECTING){
                    ctx->lcd->printLine(0, "Wifi");
                    ctx->lcd->printLine(1, "Conn...");
                }else if(st.state == WIFI_STATE_ERROR){
                    ctx->lcd->printLine(0, "Wifi NG");
                    ctx->lcd->printLine(1, "Error");
                } else {
                    ctx->lcd->printLine(0, "Wifi");
                    ctx->lcd->printLine(1, "WAIT");
                }
                break;
            }
            default:
                break;
            }
        }
        count++;
        led_sw();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_PERIOD_MS));
    }
}
