#include <stdio.h>
#include <cstring>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "UartDma.h"
#include "EventLogger.h"
#include "FlashDriver.h"
#include "FlashLogStorage.h"
#include "utility.h"
#include "AdcLoggerTask.h"
#include "CommandTask.h"
#include "AEADT7410.h"
#include "AQM0802.h"
#include "TemperatureTask.h"
#include "board_i2c.h"
#include "DisplayMode.h"
#include "ButtonTask.h"
#include "BME280.h"
#include "EnvironmentTask.h"
#include "WifiStatus.h"

#ifdef RP2040_DEBUG_TIMER_NO_PAUSE
/* for debug デバッグ時にタイマを止めない */
#include "hardware/structs/timer.h"
#endif
static FlashDriver* g_flash_driver = nullptr;
static FlashLogStorage* g_log_storage = nullptr;
static UartDma* g_uart_dma = nullptr;
static EventLogger* g_logger = nullptr;
extern size_t g_flash_log_dump_len;

// スタックサイズ
static constexpr uint LOGGER_TASK_STACK_WORDS           = 1024; // word
static constexpr uint COMMAND_TASK_STACK_WORDS          = 1024; // word
static constexpr uint ADCONVERT_TASK_STACK_WORDS        = 512;  // word
static constexpr uint TEMPERATURE_TASK_STACK_WORDS      = 768;  // word
static constexpr uint BUTTON_TASK_STACK_WORDS           = 512;
// タスク優先度: 0(最小) 〜 configMAX_PRIORITIES - 1(最大)
static constexpr UBaseType_t LOGGER_TASK_PRIORITY       = 3;
static constexpr UBaseType_t COMMAND_TASK_PRIORITY      = 2;
static constexpr UBaseType_t ADCONVERT_TASK_PRIORITY    = 1;
static constexpr UBaseType_t TEMPERATURE_TASK_PRIORITY  = 1;
static constexpr UBaseType_t BUTTON_TASK_PRIORITY       = 1;

// Flash Log領域
static constexpr uint32_t LOG_START_ADDR = 0x00001000;
static constexpr uint32_t LOG_END_ADDR   = 0x00101000;


static AppContext app_ctx;
static QueueHandle_t display_mode_queue = nullptr;
static void getFreeHeapSize(const char *s)
{
    printf("%s : %lu\n", s, static_cast<unsigned long>(xPortGetFreeHeapSize()));
}

int main()
{
    stdio_init_all();
#ifdef RP2040_DEBUG_TIMER_NO_PAUSE
    timer_hw->dbgpause = 0;
    timer_hw->pause = 0;
#endif
    sleep_ms(2000);

    printf("start log \r\n");

    /* ----------------------------------*/
    /* === UART DMA                  === */
    /* ----------------------------------*/
    static UartDma uart_dma(uart0,
        UartDma::UART_BAUDRATE_460800,
        UartDma::UART_TX_PIN,
        UartDma::UART_RX_PIN);
    if(!uart_dma.init()){
        printf("!!! UartDma.init() failed\n");
        while(true)     tight_loop_contents();
    }
    g_uart_dma = &uart_dma;

    /* ----------------------------------*/
    /* === SPI Flash                 === */
    /* ----------------------------------*/
    static FlashDriver flash(spi0,
        FlashDriver::PIN_SPI_CS,
        FlashDriver::PIN_SPI_SCK,
        FlashDriver::PIN_SPI_MOSI,
        FlashDriver::PIN_SPI_MISO,
        FlashDriver::SPI_BAUDRATE_10M);
    if(!flash.init()){
        printf("!!! FlashDriver.init() failed\n");
        while(true)     tight_loop_contents();
    }
    g_flash_driver = &flash;

    /* ----------------------------------*/
    /* === Shared IO Mutex           === */
    /* ----------------------------------*/
    // I2C通信とFlash書き込みを同時に実行させないための共通Mutex
    SemaphoreHandle_t shared_io_mutex = xSemaphoreCreateMutex();
    if(shared_io_mutex == nullptr){
        printf("!!! xSemaphoreCreateMutex(shared_io) failed\n");
        while(true)     tight_loop_contents();
    }
    
    /* ----------------------------------*/
    /* === Flashストレージ           === */
    /* ----------------------------------*/
    static FlashLogStorage storage(flash, LOG_START_ADDR, LOG_END_ADDR, shared_io_mutex);
    if(!storage.init()){
        printf("!!! storage.init() : failed\n");
        while(true)     tight_loop_contents();
    }
    g_log_storage = &storage;
    
    /* ----------------------------------*/
    /* === EventLogger               === */
    /* ----------------------------------*/
    static EventLogger logger(uart_dma, &storage);
    if(!logger.init(32)){
        printf("!!! logger.init() failed\n");
        while(true)     tight_loop_contents();
    }
    if(storage.getCount() > 0){
        logger.setNextSeq(storage.getNewestSeq() + 1);
        printf(
            "logger seq restored to %lu\r\n",
            static_cast<unsigned long>(storage.getNewestSeq() + 1)
        );
    }
    g_logger = &logger;
    
    /* ----------------------------------*/
    /* === I2C shared bus            === */
    /* ----------------------------------*/
    // I2C: AE-ADT7410 と AE-AQM0802 で共有
    board_i2c_init(i2c0, I2C_SDA_PIN, I2C_SCL_PIN, I2C_BAUDRATE);
    scan_i2c_bus(i2c0);
    
    static AEADT7410 adt7410(i2c0, AEADT7410::DEFAULT_ADDR, shared_io_mutex);
    static AQM0802 aqm0802(i2c0, AQM0802::DEFAULT_ADDR, shared_io_mutex);
    static BME280 bme280(i2c0, BME280::ADDR_0x76, shared_io_mutex);
    if(!bme280.init()){
        printf("!!! BME280 init failed\n");
    } else {
        BME280Values values;
        if(bme280.read(values)){
        printf(
            "BME280 T=%.2f C H=%.2f %% P=%.2f hPa\n",
            values.temperature_c,
            values.humidity_rh,
            values.pressure_hpa);
        }
    }
    /* ----------------------------------*/
    /* === DisplayMode Queue         === */
    /* ----------------------------------*/
    display_mode_queue = xQueueCreate(1, sizeof(DisplayMode));

    if(display_mode_queue == nullptr){
        printf("!!! xQueueCreate(display_mode_queue) failed\n");
        while(true)     tight_loop_contents();
    }
    /* ============================== */
    /* === logger task            === */
    /* ============================== */
    BaseType_t ok = xTaskCreate(EventLogger::taskEntry, "logger", LOGGER_TASK_STACK_WORDS, &logger, LOGGER_TASK_PRIORITY, nullptr);
    if(ok != pdPASS){
        printf("!!! xTaskCreate(logger) failed\n");
        while(true)  tight_loop_contents();
    }
    getFreeHeapSize("Logger task");
    /* ============================== */
    /* === Environment task            === */
    /* ============================== */
    static EnvironmentTaskContext environment_ctx ={ &bme280, &logger, 5000 };
    ok = xTaskCreate(environment_task, "environment", 2048, &environment_ctx, 1, nullptr);

    if(ok!=pdPASS){
        printf("!!! xTaskCreate(environment_task) failed\n");
        while(true) tight_loop_contents();
    }
    getFreeHeapSize("Environment task");
    /* ============================== */
    /* === ADCタスク              === */
    /* ============================== */
    ok = xTaskCreate(adc_task, "ADCtask", ADCONVERT_TASK_STACK_WORDS, &logger, ADCONVERT_TASK_PRIORITY, nullptr);
    if(ok != pdPASS){
        printf("!!! xTaskCreate(adc_task) failed\n");
        while(true)     tight_loop_contents();
    }
    getFreeHeapSize("ADconverter task");

    /* ============================== */
    /* === Command Task           === */
    /* ============================== */
    app_ctx.storage = &storage;
    app_ctx.uart = &uart_dma;
    app_ctx.logger = &logger;

    ok = xTaskCreate(command_task, "CMDtask", COMMAND_TASK_STACK_WORDS, &app_ctx, COMMAND_TASK_PRIORITY, nullptr);

    if(ok != pdPASS){
        printf("!!! xTaskCreate(command_task) failed\n");
        while(true)     tight_loop_contents();
    }
    getFreeHeapSize("Command task");

    /* ============================== */
    /* === Temperature Task       === */
    /* ============================== */
    static TemperatureTaskContext temperature_ctx = {
        &adt7410,
        &aqm0802,
        &logger,
        &storage,
        &bme280,
        display_mode_queue};
    ok = xTaskCreate(temperature_task, "temperature", TEMPERATURE_TASK_STACK_WORDS, &temperature_ctx, TEMPERATURE_TASK_PRIORITY, nullptr);
    if(ok != pdPASS){
        printf("!!! xTaskCreate(temperature_task) failed\n");
        while(true)     tight_loop_contents();
    }
    getFreeHeapSize("Temperature task");

    /* ============================== */
    /* === Button Task            === */
    /* ============================== */
    static ButtonTaskContext button_ctx = {
        display_mode_queue
    };
    ok = xTaskCreate(button_task,"button", BUTTON_TASK_STACK_WORDS, &button_ctx, BUTTON_TASK_PRIORITY, nullptr);
    if(ok != pdPASS){
        printf("!!! xTaskCreate(button_task) failed\n");
        while(true)     tight_loop_contents();
    }
    getFreeHeapSize("Button task");

    /* ============================== */
    /* === Wifi Task            === */
    /* ============================== */
    ok = xTaskCreate(wifi_task, "WiFi", 4096, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    printf("WiFi task create ret=%ld heap=%u min=%u\n",
        ok,
        xPortGetFreeHeapSize(),
        xPortGetMinimumEverFreeHeapSize());
    if(ok != pdPASS){
        printf("!!! xTaskCreate(wifi_task) failed\n");
        while(true)     tight_loop_contents();
    }
    getFreeHeapSize("Wifi task");

    /* ------------------- */
    /* スケジューラ        */
    /* ------------------- */
    vTaskStartScheduler();

    /* 通常ここは来ない */
    printf("!!! Scheduler return\n");
    while(true) tight_loop_contents();
}
