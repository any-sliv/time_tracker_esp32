#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include "iostream"
#include <chrono>
#include "esp_log.h"

using namespace std::literals::chrono_literals;

typedef std::chrono::time_point<std::chrono::system_clock> Timestamp;
typedef std::chrono::system_clock Clock;

/**
 * Pass me chrono time literal and I will return RTOS Ticks required to delay/wait.
 * @param time of type std::literals::chrono_literals
 */
template<typename T>
constexpr TickType_t ConvertToTicks(T time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time).count() / portTICK_PERIOD_MS;
}

template<typename T>
constexpr TickType_t ConvertToMs(T time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
}

template<typename T>
constexpr void TaskDelay(T time) {
    vTaskDelay(ConvertToTicks(time));
}

/****************************** END OF FILE  *********************************/