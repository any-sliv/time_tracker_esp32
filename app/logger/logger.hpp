/**
 * @file Logger.hpp
 *
 * @brief Logger header file
 *
 * @author macsli
 * @date 2022.05.25
 * @version v1.0
 *
 * @copyright 2022 Fideltronik R&D - all rights reserved.
 */

#ifndef INC_LOGGER_H_
#define INC_LOGGER_H_

#pragma once

/******************************************************************************
                                 INCLUDES
******************************************************************************/

#include <iostream>
#include <string>
#include <sstream>

extern "C" {
#include "esp_log.h"
} // extern C close

/******************************************************************************
                             MACROS/DEFINITIONS
******************************************************************************/

namespace Logger {

    namespace LoggerHelper {
        /**
        * Recursive function for variadic template (...)
        *
        * @param t types:char/string/<numeric>
        * @return string
        */
        template<typename T>
        std::string concatenate(T t) {
            std::stringstream ss;
            ss << t;
            // Add a space at end of each arg
            return std::string(ss.str() + " ");
        }

        /* If build fails in this file you probably given wrong param type to logger */

        /**
        * Helper function for LOG_ 
        *
        * @param args variable number of parameters of type char/string/<numeric>
        * @return concatenated string of all given parameters
        */
        template<typename T, typename... Args>
        std::string concatenate(T first, Args... args) {
            std::string text = concatenate(first);
            text.append(concatenate(args...));
            return text;
        }
    } // LoggerHelper NAMESPACE
    
    /**
    * Info logger. Give me anything and I will log it!
    *
    * @param args any number of parameters (type char/string/<numeric>)  
    */
    template<typename... Args>
    void LOGI(Args... args) {
        std::string text = LoggerHelper::concatenate(args...);
        ESP_LOGI("", "%s", text.c_str());
    }

    /**
    * Error logger. Give me anything and I will log it!
    *
    * @param args any number of parameters (type char/string/<numeric>)  
    */
    template<typename... Args>
    void LOGE(Args... args) {
        std::string text = LoggerHelper::concatenate(args...);
        ESP_LOGE("", "%s", text.c_str());
    }

    /**
    * Warning logger. Give me anything and I will log it!
    *
    * @param args any number of parameters (type char/string/<numeric>)  
    */
    template<typename... Args>
    void LOGW(Args... args) {
        std::string text = LoggerHelper::concatenate(args...);
        ESP_LOGW("", "%s", text.c_str());
    }
}

#endif
/****************************** END OF FILE  *********************************/
