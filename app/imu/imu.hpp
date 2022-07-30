#pragma once

#include "mpu6050.hpp"
#include <array>
#include "dateTime.hpp"
#include "nvs.hpp"

namespace IMU {

void ImuTask(void *pvParameters);

//flash contains N entries of structs (position, cubeFaceNo)
//position is X,Y,Z
//at startup you do calibration, face number is assigned at each side facing downwards
//N positions are detected and N faces are assigned

//Flash[N] = {X, Y, Z, faceNumber}

//there must be a function getFaceNumber
//which will read from flash all registered positions
//will get current poistion as a parameter
//compare current position with saved ones
//and return face number


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

    // x, y, z
    std::array<float, 3> pos;

    /**
     * "==" operator checks if value is in bound +/- x% of compared one
     * @return close enough?
     */
    bool operator == (Orientation orient) {
        float range = 0.9; // x%

        for(unsigned int i = 0; i < pos.size(); i++ ) {
            if(!(pos[i] > (orient.pos[i] - orient.pos[i] * range)) &&
                (pos[i] < (orient.pos[i] + orient.pos[i] * range))) {
                    return false;
                }
        }
        return true;
    }
};

class Imu : public MPU6050 {

    bool isCalibrated;
    bool checkCalibration();

public:
    NVS::Nvs nvs;
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

    void Calibrate();

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