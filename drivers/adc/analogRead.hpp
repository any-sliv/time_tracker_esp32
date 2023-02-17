/*
 * analogRead.hpp
 *
 *  Created on: Oct 27, 2021
 *      Author: macsli
 */

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/adc_common.h"
#include "esp_log.h"

#if CONFIG_IDF_TARGET_ESP32
#define ADC_EXAMPLE_CALI_SCHEME     ESP_ADC_CAL_VAL_EFUSE_VREF
#elif CONFIG_IDF_TARGET_ESP32S2
#define ADC_EXAMPLE_CALI_SCHEME     ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32C3
#define ADC_EXAMPLE_CALI_SCHEME     ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32S3
#define ADC_EXAMPLE_CALI_SCHEME     ESP_ADC_CAL_VAL_EFUSE_TP_FIT
#endif

class AnalogRead {
private:
    adc1_channel_t pin;
    esp_adc_cal_characteristics_t adc1_chars;

public:
    AnalogRead(adc1_channel_t _pin) : pin(_pin) {
        esp_err_t ret;

        ret = esp_adc_cal_check_efuse(ADC_EXAMPLE_CALI_SCHEME);
        if (ret == ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(__FILE__, "Calibration scheme not supported, skip software calibration");
        } else if (ret == ESP_ERR_INVALID_VERSION) {
            ESP_LOGW(__FILE__, "eFuse not burnt, skip software calibration");
        } else if (ret == ESP_OK) {
            esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db, (adc_bits_width_t) ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);
        } else {
            ESP_LOGE(__FILE__, "Invalid arg");
        }

        adc1_config_width(ADC_WIDTH_12Bit);
        // Careful - attenuation does reduce input voltage range
        adc1_config_channel_atten(pin, ADC_ATTEN_DB_11);
    }

    /**
    * Read value from ADC input
    * @return value of ADC measurement
    */
    int GetAdcValue() {
        return adc1_get_raw(pin);
    }

    float GetAdcVoltage() {
        return (float)(esp_adc_cal_raw_to_voltage(GetAdcValue(), &adc1_chars)) / 1000;
    }
};
