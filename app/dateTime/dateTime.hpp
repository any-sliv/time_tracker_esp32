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
#include "Logger.hpp"

extern "C" {
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "esp_sntp.h"
} // extern C close

/******************************************************************************
                             MACROS/DEFINITIONS
******************************************************************************/

/******************************************************************************
                                 FUNCTIONS
******************************************************************************/

/**
* Class DateTime. Stores time in private member. 
* Object creation automatically initializes SNTP server and sets time to current system time. 
* If no SNTP server is synced, default time value is 2000/01/01,00:00:00. 
*/
class DateTime {
    time_t timestamp;

public:
    DateTime(const std::string sntpServerName = "pool.ntp.org", time_t defaultTime = 946684800) {
        Logger::LOGI("DateTime", "Init.");
        struct timeval tv;
        tv.tv_sec = defaultTime;
        timestamp = defaultTime;
        // set default time
        settimeofday(&tv, NULL);

        // update timestamp
        SetNow();

        // if operating mode is set sntp is already running
        // opmod is set and you try to set again - core panic
        if(sntp_getoperatingmode() != SNTP_OPMODE_POLL) {
            return;
        }
        
        Logger::LOGI("DateTime", "SNTP init");
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, sntpServerName.c_str());
        sntp_init();

        //todo add location fetching and adjust tz accordingly?
        SetTimeZone();
    }

    /**
    * Get current system time. Time is saved in private member.
    */
    void SetNow(void) {
        timestamp = time(NULL);
    }

    std::string GetTimeStringFormat(void) {
        char CurrentTimeUtc[25];
        struct tm timeinfo;
        localtime_r(&timestamp, &timeinfo);
        strftime(CurrentTimeUtc, sizeof(CurrentTimeUtc), "%F %T", &timeinfo);
        return std::string(CurrentTimeUtc);
    }

    /**
    * Set time zone
    *
    * @param timezone string. Default is "UTC" timezone.
    * Examples: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv "Europe/Warsaw".
    */
    void SetTimeZone(const std::string tz = "UTC") {
        setenv("TZ", tz.c_str(), 1);
        tzset();
    }

    //todo date addition
    
    /**
    * Subtraction operator. Result of (end-beginning) in seconds as a value of type double.
    * result = <end> - <beginning>
    */
    double operator - (DateTime const &obj) {
        return difftime(timestamp, obj.timestamp);
    }

    /**
    * Subtraction operator. Result of (end-beginning) in seconds as a value of type double.
    * result = <end> - <beginning>
    */
    double operator - (int seconds) {
        return difftime(timestamp, (time_t) seconds);
    }
};


#endif
/****************************** END OF FILE  *********************************/
