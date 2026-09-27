/**
 * @file imu.hpp
 * @author Maciej Sliwinski
 * @brief This file is a part of time_tracker_esp32 project.
 *
 * The code is distributed under the MIT License.
 * See the LICENCE file for more details.
 */

#pragma once

#include "mpu6050.hpp"
#include <array>
#include <cstring>
#include "dateTime.hpp"
#include "gpio.hpp"
#include "nvs.hpp"

namespace IMU {

/**
 * @brief IMU device task/thread. Execution managed by OS.
 */
void ImuTask(void *pvParameters);

struct PositionQueueType {
    PositionQueueType() {};
    PositionQueueType(int _face, int _time) : face(_face), startTime(_time) {};

    unsigned int face;
    time_t startTime;
};

/**
 * @brief Size of the buffer carrying a formatted batch of position records
 *      (ImuPositionQueue item size). Both endpoints must declare a buffer of
 *      exactly this size - xQueueReceive copies the full item size regardless
 *      of what is passed to it.
 */
constexpr size_t PositionBatchMaxLen = 256;

template<typename Type, int SizeT>
class SimpleVector {
    unsigned int activeItems;
    std::array<Type, SizeT> elements;
public:
    // Leave a default construtor if class is used with RTC_DATA_ATTR

    constexpr int Size() {
        return SizeT;
    }

    int GetActiveItems() {
        return activeItems;
    }

    // Items live at indices [1..activeItems]; index 0 is never written.
    // Do NOT re-base this to 0: RTC memory survives the post-OTA deep sleep, so new
    // firmware would misread records written by the old one.

    Type Pop() {
        if(activeItems == 0) {
            // Prevent from returning item with index -1
            return elements[0];
        }
        Type retItem = elements[activeItems];
        activeItems--;
        return retItem;
    }

    /**
     * @brief Oldest item, without removing it. Caller MUST check GetActiveItems()
     *      first - on an empty vector this returns never-written elements[0].
     */
    Type PeekOldest() {
        return elements[1];
    }

    /**
     * @brief Discard the oldest item, shifting the rest down one slot.
     */
    void DropOldest() {
        if(activeItems == 0) {
            return;
        }
        // Shift [2..activeItems] down onto [1..activeItems-1]
        memmove(&elements[1], &elements[2], (activeItems - 1) * sizeof(Type));
        activeItems--;
    }

    /**
     * @brief FIFO pop. Pop() is LIFO (newest first); this returns the OLDEST item,
     *      so draining yields chronological order. Used when batching records to BLE.
     */
    Type PopOldest() {
        if(activeItems == 0) {
            return elements[0];
        }
        Type retItem = PeekOldest();
        DropOldest();
        return retItem;
    }

    void Push(Type item) {
        if(activeItems + 1 >= SizeT) {
            // Highest writable slot is SizeT-1; '>' here wrote elements[SizeT],
            // one past the end, corrupting whatever RTC var followed.
            return;
        }
        elements[activeItems + 1] = item;
        activeItems++;
    }

    bool isDuplicate(Type item) {
        if(!activeItems) {
            return false;
        }

        // Data lives at [1..activeItems]; iterating from 0 compared uninitialised
        // elements[0] and never checked the newest entry.
        for(unsigned int i = 1; i <= activeItems; i++) {
            if(elements[i] == item) {
                return true;
            }
        }
        return false;
    }
};

// Used for x,y,z position data operation
class Orientation {
public:
    Orientation() {};

    Orientation(float x, float y, float z) : pos({x, y, z}) {};

    std::array<float, 3> pos; // x, y, z

    /**
     * "==" operator checks if value is in bound +/- x of compared one
     * @return close enough?
     */
    bool operator == (Orientation newPos) {

        constexpr float drift = 0.15;

        for(auto i = 0; i < pos.size(); i++ ) {
            if(newPos.pos[i] > ( pos[i] + drift) ||
                (newPos.pos[i]) < (pos[i] - drift)) {
                    //if exceeded range, the value is not equal
                    return false;
            }
        }
        return true;
    }

    void PrintPosition(int line = __LINE__) {
        ESP_LOGI(__FILE__, "%s:%d. Position: (%.4f),(%.4f),(%.4f)", __func__ , line, pos[0], pos[1], pos[2]);
    }
};

class Imu : private MPU6050 {
public:
    const static constexpr std::chrono::milliseconds taskPeriod = 5ms;
    const static constexpr std::chrono::seconds rollCooldown = 5s; // registering new position cooldown
    const static constexpr int cubeFaces = 9;

    Imu();

    /**
     * @brief Perform calibration process of face position. 
     *      One face register on function call.
     *      Positions temporarily saved in RTC_DATA_ATTR. 
     *      When all faces registered - they're saved in flash.
     * @return see enum CalibrationStatus
     */
    int CalibrateCubeFaces(int& returnPosition);

    /**
     * @brief Get position (X, Y, Z) from imu accelometer. 
     * @return (Orientation) object.
     */
    Orientation GetPositionRaw(void);

    /**
     * @brief Saves position in RTCdata memory.
     * @param newOrient new orientation
     * @return bool if position is accepted (1 success)
     */
    bool OnPositionChange(const Orientation& newOrient);

    /**
     * @brief Pass the current orientation, func will return
     *      face number the cube is facing upwards
     * @param orient struct, current orientation
     * @return Face number (>0), or -1 in case of error/not calibrated
     */
    int DetectFace(const Orientation& orient);

private:
    const static constexpr gpio_num_t pinSda = (gpio_num_t) 21; //23lolin(?gnd) //21firebeetle
    const static constexpr gpio_num_t pinScl = (gpio_num_t) 22; //19lolin(?gnd) //22firebeetle
    const static constexpr i2c_port_t port = (i2c_port_t) I2C_NUM_0;

    NVS::Nvs nvs;

    enum CalibrationStatus {
        ERROR = -1,
        IDLE,   // Waiting for input from app
        ALREADY_EXISTS, // Detected pos already exists in memory
        OK, // Position registered correctly
        DONE // Calibration process done
    };

    int getNoOfCalibratedPositions();

    /**
     * @brief Check of calibration
     * @return true if all positions are registered
     */
    bool checkCalibration();
    
    /**
     * @brief Check of calibration
     * @param orient position to be checked
     * @return true if given position exists in memory as calibrated
     */
    bool checkCalibration(const Orientation& orient);

    /**
     * @brief Clear all registered positions from flash memory
     * @return true if success
     */
    esp_err_t eraseCalibration();
};

}; // Namespace end ------------------
