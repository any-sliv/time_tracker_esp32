/**
 * @file battery.hpp
 * @author Maciej Sliwinski
 * @brief This file is a part of time_tracker_esp32 project.
 *
 * The code is distributed under the MIT License.
 * See the LICENCE file for more details.
 */

#pragma once

#include "array"
#include "gpio.hpp"
#include "analogRead.hpp"

namespace BATTERY {

/**
 * @brief Battery task/thread. Execution managed by OS.
 */
void BatteryTask(void *pvParameters);

class Battery : private AnalogRead {
private:
    // Each value represents step of 10% in capacity
    std::array<float, 12> voltageCx = {2.5, 3.65, 3.69, 3.74, 3.8, 3.84,
                          3.87, 3.95, 4.02, 4.10, 4.2, 4.3};

public:
    Battery() = delete;
    
    Battery(adc_unit_t unit, adc_channel_t channel) : AnalogRead(unit, channel) {};
    
    /**
     * @return Percentage value of battery. '0' or multiple of 10
     */
    int GetPercent();

    // ChargeState GetChargeState(void);
};

}