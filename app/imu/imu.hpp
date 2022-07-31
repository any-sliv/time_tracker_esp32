#pragma once

#include "mpu6050.hpp"
#include <array>
#include "dateTime.hpp"
#include "nvs.hpp"

namespace IMU {

void ImuTask(void *pvParameters);

class Orientation {
public:
    Orientation() {};

    Orientation(float x, float y, float z) {
        pos = {x, y, z};
    }
    // Copy constructor
    Orientation(const Orientation& orient) {
        pos = orient.pos;
    }

    std::array<float, 3> pos; // x, y, z

    /**
     * "==" operator checks if value is in bound +/- x% of compared one
     * @return close enough?
     */
    bool operator == (Orientation orient) {
        constexpr float range = 0.9f; // x%

        for(auto i = 0; i < pos.size(); i++ ) {
            if(!(pos[i] > (orient.pos[i] - orient.pos[i] * range)) &&
                (pos[i] < (orient.pos[i] + orient.pos[i] * range))) {
                    return false;
                }
        }
        return true;
    }
};

class Imu : public MPU6050 {
private:
    bool isCalibrated;
    bool checkCalibration();

    // This stores positions registered in calibration.
    std::array<Orientation, IMU::Imu::cubeFaces> savedPositions;
    // NVS flash object
    NVS::Nvs nvs;

public:
    const static constexpr gpio_num_t pinSda = (gpio_num_t) 14;
    const static constexpr gpio_num_t pinScl = (gpio_num_t) 12;
    const static constexpr i2c_port_t port = (i2c_port_t) I2C_NUM_1;

    const static constexpr int taskPeriod = ConvertToTicks(100ms);
    const static constexpr int cubeFaces = 4;
    const static constexpr std::chrono::seconds rollCooldown = 2s; // registering new position cooldown

    //todo static variable defining current state of imu?
    //todo and use this instead of queuing between tasks???
    static int x;

    Imu();

    /**
     * @brief Perform calibration process. At some input app registers current imu position
     *      and saves them in flash memory.
     */
    void Calibrate();

    /**
     * @brief Get position from imu (X, Y, Z) accelometer.
     * @return (Orientation) object.
     */
    Orientation GetPosition(void);

    /**
     * @brief Pass the current orientation, func will return
     *      face number the cube is facing upwards
     * @param orient struct, current orientation
     * @return Face number
     */
    int DetectFace(Orientation orient);
};

}; // Namespace end ------------------
