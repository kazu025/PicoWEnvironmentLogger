
#include "ButtonTask.h"
#include "DisplayMode.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <cstdio>

static constexpr uint BUTTON_PIN = 15;
static constexpr uint32_t DEBOUNCE_MS = 50;
static constexpr uint32_t POLL_MS = 10;

void button_task(void* param)
{
    auto* ctx = static_cast<ButtonTaskContext*>(param);

    if(ctx == nullptr || ctx->display_mode_queue == nullptr){
        printf("button_task: invalid context\n");
        vTaskDelete(nullptr);
    }

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    bool prev_state = gpio_get(BUTTON_PIN);
    DisplayMode mode = DisplayMode::Temperature;

    printf("button_task: start GPIO%u\n", BUTTON_PIN);

    while(true){
        bool now_state = gpio_get(BUTTON_PIN);

        // プルアップ入力なので true -> false が押下
        if(prev_state == true && now_state == false){
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));

            // debounce後も押されていれば有効
            if(gpio_get(BUTTON_PIN) == false){
                int next = static_cast<int>(mode) + 1;

                if(next >= static_cast<int>(DisplayMode::Max)){
                    next = 0;
                }

                mode = static_cast<DisplayMode>(next);

                BaseType_t ret = xQueueOverwrite(
                    ctx->display_mode_queue,
                    &mode
                );

                if(ret == pdPASS){
                    printf("button_task: mode=%d\n", static_cast<int>(mode));
                }else{
                    printf("button_task: queue send failed\n");
                }

                // ボタンが離されるまで待つ
                while(gpio_get(BUTTON_PIN) == false){
                    vTaskDelay(pdMS_TO_TICKS(POLL_MS));
                }

                // 離した後のチャタリング待ち
                vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));

                prev_state = true;
                continue;
            }
        }

        prev_state = now_state;
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}