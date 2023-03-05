/*
 * battery.cpp
 *
 *  Created on: 31 Jan, 2023
 *      Author: macsli
*/

#include "battery.hpp"
#include "dateTime.hpp"

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/queue.h"
  #include "freertos/task.h"
} // extern C close

using namespace BATTERY;

extern QueueHandle_t BatteryQueue;

void BATTERY::BatteryTask(void *pvParameters) {
    ESP_LOGI(__FILE__, "%s:%d. Task init", __func__ ,__LINE__);

    // see ADCx_CHANNEL macro description for details
    // Battery battery(ADC1_CHANNEL_6); // Lolinlite

    //TODO lolin has no battery read pin
    Battery battery(ADC_UNIT_1, ADC_CHANNEL_6); // Firebeetle

    for(;;) {
        auto batPercent = battery.GetPercent();
        if(xQueueSend(BatteryQueue, &batPercent, 0) == pdFALSE) {
            // overwrite existing value
            xQueueOverwrite(BatteryQueue, &batPercent);
        }
        
        TaskDelay(1s);
    }
}

int Battery::GetPercent() {
    // theres two stage voltage divider on battery voltage
    // 1st *0.5, 2nd *0.3125
    // Vout(@adc_pin) = Vbat * 0.1563
    // Vbat = Vout(@adc_pin) / 0.1563; 1/0.1563 = 6.4
#ifdef ARDUINO_LOLIN32_LITE
    auto voltage = GetAdcVoltage() * 6.4;
#endif
    //firebeetle voltage divier: /2
// #ifdef ARDUINO_FIREBEETLE32
    auto voltage = GetAdcVoltage() * 2;
// #endif

    for (uint8_t i = 0; i < voltageCx.size(); i++) {
        if(voltage > voltageCx[i]) {
            if(voltage < voltageCx[i + 1]) {
                return i * 10;
            }
        }
    }
    return 0;
}
