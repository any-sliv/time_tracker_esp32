#pragma once

#include "mpu6050.hpp"
#include <array>
#include "dateTime.hpp"
#include "gpio.hpp"
#include "nvs.hpp"
#include "NimBLECharacteristic.h"

namespace IMU {

/**
 * @brief IMU device task/thread. Execution managed by OS.
 */
void ImuTask(void *pvParameters);

struct PositionQueueType {
    PositionQueueType() : face(0), startTime(0) {};
    PositionQueueType(int _face, int _time) : face(_face), startTime(_time) {};
    unsigned int face;
    time_t startTime;
};

// User is responsible for not exceeding size boundaries when using Pop() or Push()
// This "vector" is implementation specific. Not a generic class for any type (I tried)
template<typename T>
class SimpleVector {
    int activeItems = 0;
    std::array<T, 50> elements;
public:
    SimpleVector() {};

    size_t size() {
        return elements.size();
    }

    int GetActiveItems() {
        return activeItems;
    }

    T Pop() {
        auto retItem = elements[activeItems];
        // Yes. That's how I clear an element.
        elements[activeItems].face = 0;
        // ----------------------------------
        activeItems--;
        return retItem;
    }

    void Push(T &item) {
        elements[activeItems + 1] = item;
        activeItems++;
    }
};

// Used for x,y,z position data operation
class Orientation {
public:
    Orientation() {};

    Orientation(float x, float y, float z) : pos({x, y, z}) {};
    // Copy constructor
    Orientation(const Orientation& orient) : pos(orient.pos) {};

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
    const static constexpr std::chrono::milliseconds taskPeriod = 1ms;
    const static constexpr std::chrono::seconds rollCooldown = 5s; // registering new position cooldown
    const static constexpr int samplesFilterSize = 5;

    Imu();

    /**
     * @brief Perform calibration process of face position. 
     *      One face register on function call. Saved in flash at success
     * @return -1 - error, 0 - current position exists, 1 - position saved, 2 - calibration done
     */
    int CalibrateCubeFaces();

    /**
     * @brief Get position from imu (X, Y, Z) accelometer. 
     * @return (Orientation) object.
     */
    Orientation GetPositionRaw(void);

    /**
     * @brief Saves position in RTCdata memory (NVS).
     * @param newOrient new orientation
     */
    void OnPositionChange(const Orientation& newOrient);

private:
    const static constexpr int cubeFaces = 3;
    const static constexpr gpio_num_t pinSda = (gpio_num_t) 14;
    const static constexpr gpio_num_t pinScl = (gpio_num_t) 12;
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
    bool eraseCalibration();

    /**
     * @brief Pass the current orientation, func will return
     *      face number the cube is facing upwards
     * @param orient struct, current orientation
     * @return Face number (>0), or -1 in case of error
     */
    int DetectFace(const Orientation& orient);
};

}; // Namespace end ------------------
