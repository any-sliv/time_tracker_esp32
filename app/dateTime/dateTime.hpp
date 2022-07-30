#pragma once

#ifndef INC_DATETIME_HPP_
#define INC_DATETIME_HPP_

#include "iostream"
#include "logger.hpp"
#include <chrono>

extern "C" {
#include <time.h>
#include "esp_sntp.h"
} // extern C close

using namespace std::literals::chrono_literals;

typedef std::chrono::time_point<std::chrono::steady_clock> Timestamp;
typedef std::chrono::steady_clock Clock;

/**
 * Pass me chrono time literal and I will return RTOS Ticks required to delay/wait.
 * @param time of type std::literals::chrono_literals
 */
template<typename T>
constexpr TickType_t ConvertToTicks(T time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time).count() / portTICK_RATE_MS;
}

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
        Logger::LOGI("SNTP", __func__, ":", __LINE__, "Init");
        struct timeval tv;
        tv.tv_sec = defaultTime;
        // set default time
        settimeofday(&tv, NULL);

        // Prevent SNTP from reinitialising (can crash).
        sntp_stop();
        
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, sntpServerName.c_str());
        sntp_init();

        //todo add location fetching and adjust tz accordingly?
        setTimeZone();
    }

    /** 
     * Get a reference to the singleton instance
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
