/*
 * analogRead.hpp
 *
 *  Created on: Oct 27, 2021
 *      Author: macsli
 */


extern "C" {
    #include "esp_adc/adc_oneshot.h"
    #include "esp_adc/adc_cali.h"
    #include "esp_adc/adc_cali_scheme.h"
    #include "esp_log.h"
} // extern C close



class AnalogRead {
private:
    adc_unit_t unit;
    adc_channel_t channel;
    adc_oneshot_unit_handle_t handle;
    adc_cali_handle_t cali;

public:
    AnalogRead() = delete;
    AnalogRead(adc_unit_t _unit, adc_channel_t _channel) : unit(_unit), channel(_channel) {
        esp_err_t ret = ESP_FAIL;
        bool calibrated;
        adc_oneshot_unit_init_cfg_t adcConfig;
        adcConfig.unit_id = unit;
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&adcConfig, &handle));

        adc_oneshot_chan_cfg_t config {.atten = ADC_ATTEN_DB_11, .bitwidth = ADC_BITWIDTH_DEFAULT};
        ESP_ERROR_CHECK(adc_oneshot_config_channel(handle, channel, &config));

    #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        if (!calibrated) {
            adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = unit,
                .atten = atten,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
            if (ret == ESP_OK) {
                calibrated = true;
            }
        }
    #endif

    #if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        if (!calibrated) {
            adc_cali_line_fitting_config_t cali_config = {
                .unit_id = unit,
                .atten = ADC_ATTEN_DB_11,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            ret = adc_cali_create_scheme_line_fitting(&cali_config, &cali);
            if (ret == ESP_OK) {
                calibrated = true;
            }
        }
    #endif
        if (ret == ESP_OK) {
            ESP_LOGI("ADC", "Calibration Success");
        } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
            ESP_LOGW("ADC", "eFuse not burnt, skip software calibration");
        } else {
            ESP_LOGE("ADC", "Invalid arg or no memory");
        }
    };

    /**
    * Read value from ADC input
    * @return value of ADC measurement
    */
    int GetAdcValue() {
        int out;
        adc_oneshot_read(handle, channel, &out);
        return out;
    }

    float GetAdcVoltage() {
        int out;
        adc_cali_raw_to_voltage(cali, GetAdcValue(), &out);
        float volt = (float)out;
        return volt / 1000;
    }
};
