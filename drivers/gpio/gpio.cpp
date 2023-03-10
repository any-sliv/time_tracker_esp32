/**
 * @file main.cpp
 * @author Maciej Sliwinski
 * @brief This file is a part of time_tracker_esp32 project.
 *
 * The code is distributed under the MIT License.
 * See the LICENCE file for more details.
 */

#include "gpio.hpp"

//TODO you should define copy-constructor

Gpio::Gpio(int _pin, Mode mode, bool initState, Pull pull) {
    pin = (gpio_num_t) _pin;
    gpio_pullup_t pu = GPIO_PULLUP_DISABLE;
    gpio_pulldown_t pd = GPIO_PULLDOWN_DISABLE;

    if(pull == NO_PULL) {
        pu = GPIO_PULLUP_DISABLE;
        pd = GPIO_PULLDOWN_DISABLE;
    }
    else if(pull == PULLUP) {
        pu = GPIO_PULLUP_ENABLE;
        pd = GPIO_PULLDOWN_DISABLE;
    }
    else if(pull == PULLDOWN) {
        pu = GPIO_PULLUP_DISABLE;
        pd = GPIO_PULLDOWN_ENABLE;
    }

    gpio_config_t pinConfig;
    pinConfig.intr_type = GPIO_INTR_DISABLE;
    pinConfig.mode = (gpio_mode_t)mode;
    pinConfig.pull_down_en = pd;
    pinConfig.pull_up_en = pu;
    pinConfig.pin_bit_mask = BIT((int)pin);
    
    // Configure pin with given configuration
    esp_err_t res = gpio_config(&pinConfig);
    ESP_ERROR_CHECK(res);

    Set(initState);
}

void Gpio::Set(void) { 
    esp_err_t res = gpio_set_level(pin, 1);
    ESP_ERROR_CHECK(res);
}

void Gpio::Set(bool value) {
    state = value;
    esp_err_t res = gpio_set_level(pin, (uint32_t) value);
    ESP_ERROR_CHECK(res);
}

void Gpio::Reset(void) { 
    esp_err_t res = gpio_set_level(pin, 0);
    ESP_ERROR_CHECK(res);
}

void Gpio::Toggle() {
    bool val = !state;
    state = val;
    esp_err_t res = gpio_set_level(pin, (uint32_t) state);
    ESP_ERROR_CHECK(res);
}

bool Gpio::Read(void) { return false; }

uint32_t Gpio::GetPinNumber(void) { return pin; }