/**
 * Project : R3 LTE - OSO Water heater.
 *
 * \file DateTime.hpp
 * \brief
 * \author macsli
 * \date 2022.05.23
 * \copyright 2022 Fideltronik R&D - all rights reserved.
 * \version File template version 12.2018
 */

#pragma once

#ifndef INC_DATETIME_H_
#define INC_DATETIME_H_

/******************************************************************************
                                 INCLUDES
******************************************************************************/

#include "iostream"
#include "logger.hpp"
#include <chrono>


extern "C" {
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "esp_sntp.h"
} // extern C close

using namespace std::literals::chrono_literals;

typedef std::chrono::time_point<std::chrono::steady_clock> Timestamp;
typedef std::chrono::steady_clock Clock;

//todo has passed? give argument of some Timestamp and compare to ::now()

/**
* Class DateTime. Stores time in private member. 
* If no SNTP server is synced, default time value is 2000/01/01,00:00:00. 
*/
class SNTP {
public:
    SNTP() = default;
    //todo add destructor

    static void init(const std::string sntpServerName = "pool.ntp.org", time_t defaultTime = 946684800) {
        Logger::LOGI("DateTime", "Init.");
        struct timeval tv;
        tv.tv_sec = defaultTime;
        // set default time
        settimeofday(&tv, NULL);

        // Prevent SNTP from reinitialising.
        sntp_stop();
        
        Logger::LOGI("DateTime", "SNTP init");
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, sntpServerName.c_str());
        sntp_init();

        //todo add location fetching and adjust tz accordingly?
        setTimeZone();
    }

    /** 
     * @brief Get a reference to the singleton instance
     * @return reference to a singleton SNTP object
     */
    static SNTP& get_instance(void)
    {
        static SNTP sntp;
        return sntp;
    }

     /**
    * Set time zone
    *
    * @param timezone string. Default is "UTC" timezone.
    * Examples: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv "Europe/Warsaw".
    */
    static void setTimeZone(const std::string tz = "UTC") {
        setenv("TZ", tz.c_str(), 1);
        tzset();
    }
};

#endif
/****************************** END OF FILE  *********************************/
