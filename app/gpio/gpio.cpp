/**
 * @file gpio.cpp
 *
 * @brief Gpio driver source file
 *
 * @author macsli
 * @date 2022.05.25
 */

#include "gpio.hpp"

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

    gpio_config_t pinConfig {
        BIT((int) pin), // check macro GPIO_SEL_
        (gpio_mode_t) mode,
        pu,
        pd,
        GPIO_INTR_DISABLE
    };
    // Configure pin with given configuration
    esp_err_t res = gpio_config(&pinConfig);
    ESP_ERROR_CHECK(res);

    Set(initState);
}

Gpio::Gpio() {};

void Gpio::Set(void) { 
    gpio_pad_select_gpio(pin);
    esp_err_t res = gpio_set_level(pin, 1);
    ESP_ERROR_CHECK(res);
}

void Gpio::Set(bool value) {
    gpio_pad_select_gpio(pin);
    esp_err_t res = gpio_set_level(pin, (uint32_t) value);
    ESP_ERROR_CHECK(res);
}

void Gpio::Reset(void) { 
    gpio_pad_select_gpio(pin);
    esp_err_t res = gpio_set_level(pin, 0);
    ESP_ERROR_CHECK(res);
}

bool Gpio::Read(void) { return false; }

uint32_t Gpio::GetPinNumber(void) { return pin; }

//TODO deconstructor