/**
 * @file main.cpp
 * @author Maciej Sliwinski
 * @brief This file is a part of time_tracker_esp32 project.
 *
 * The code is distributed under the MIT License.
 * See the LICENCE file for more details.
 */

#pragma once

#include "driver/gpio.h"


class Gpio {
private:
    gpio_num_t pin;
    bool state;
    //   GPIO_TypeDef *port;

public:
    enum Mode {
        INPUT = GPIO_MODE_INPUT,
        OUTPUT = GPIO_MODE_OUTPUT,
        OUTPUT_OD = GPIO_MODE_OUTPUT_OD
    };

    enum Pull {
        NO_PULL,
        PULLUP,
        PULLDOWN,
    };

    /**
     * Gpio class constructor. Instantize if want to initialize GPIO pin.
     *
     * @param _pin (int)
     * @param mode Input/output, default = INPUT
     * @param initState State which pin is given at init, default = 0
     * @param pull pull type, default = NO_PULL
     */
    Gpio(int _pin, Mode = INPUT, bool initState = 0, Pull = NO_PULL);

    Gpio() = delete;

    // Sets pin value to '1'
    void Set(void);

    /**
     * Sets pin value to @param value
     */
    void Set(bool value);

    // Sets pin value to '0'
    void Reset(void);

    void Toggle();

    /**
     * @retval Value of current pin state
     * WARNING! Inverted logic!
     */
    bool Read(void);

    uint32_t GetPinNumber(void);
};