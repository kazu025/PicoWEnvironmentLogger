#pragma once

#include "FreeRTOS.h"
#include "queue.h"

struct ButtonTaskContext {
    QueueHandle_t display_mode_queue;
};
void button_task(void* param);