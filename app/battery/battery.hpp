/*
 * battery.hpp
 *
 *  Created on: Oct 29, 2021
 *      Author: macsli
 */

#pragma once

#include "gpio.hpp"
#include "analogRead.hpp"
#include "NimBLECharacteristic.h"

namespace BATTERY {

/**
 * @brief Battery task/thread. Execution managed by OS.
 */
void BatteryTask(void *pvParameters);

class Battery : private AnalogRead {
private:
    // Each value represents step of 10% in capacity
    const float voltageCx[12] = {3.6,  3.65, 3.69, 3.74, 3.8, 3.84,
                          3.87, 3.95, 4.02, 4.10, 4.2, 4.3};

public:
    Battery() = delete;
    
    /**
     * @param channel pointer to ADC channel configuration structure
     * @param gpioAddr has default value, use only for testing!
     * Warning: all pins within are initialized with one default PORTx address
     * If they differ, introduce new default arguments
     */
    Battery(adc1_channel_t pin) : AnalogRead(pin) {};
    
    /**
     * @return Percentage value of battery. '0' or multiple of 10
     */
    float GetPercent();

    // ChargeState GetChargeState(void);
};

}