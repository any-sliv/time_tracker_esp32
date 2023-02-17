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

    //todo ADC DISABLED. LOLINLITE BOARD HAS NO READOUT FROM BATTERY
    // Battery battery(ADC1_CHANNEL_2); //GPIO2 (6)
    float batPercent = 35.0;

    for(;;) {
        // batPercent = battery.GetPercent();
        if(xQueueSend(BatteryQueue, &batPercent, 0) == pdFALSE) {
            //todo do something if couldnt put info?
            //overwrite existing value
        }
        
        TaskDelay(1s);
    }
}

float Battery::GetPercent() {
    // theres two stage voltage divider on battery voltage
    // 1st *0.5, 2nd *0.3125
    // Vout(@adc_pin) = Vbat * 0.1563
    // Vbat = Vout(@adc_pin) / 0.1563; 1/0.1563 = 6.4
    auto voltage = GetAdcVoltage() * 6.4;

    if(voltage > voltageCx[11]) return 101;

    for (uint8_t i = 0; i < 11; i++) {
        if (voltage > voltageCx[i]) {
            if (i == 10) return i * 10;
        }
        else if (voltage < voltageCx[i + 1]) {
            return i * 10;
        } 
        return i * 10;
    }
    return 0;
}
