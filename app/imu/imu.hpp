#pragma once

#include "mpu6050.hpp"

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


struct Orientation {
    // float x, y, z;
    // x,y,z are an abstract for us
    float pos[3];

    /**
     * @brief "==" operator checks if value is in bound +/- 10% of compared one
     */
    bool operator == (Orientation orient) {
        float range = 0.1; //10%

        for(unsigned int i = 0; i < (sizeof(pos)/sizeof(pos[0])); i++ ) {
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
    static constexpr gpio_num_t pinSda = (gpio_num_t) 25;
    static constexpr gpio_num_t pinScl = (gpio_num_t) 26;
    static constexpr i2c_port_t port = (i2c_port_t) I2C_NUM_1;

    Imu();

    void Calibrate();

    /**
     * @brief Pass the current orientation, func will return
     *      face number the cube is facing upwards
     * @param orient struct, current orientation
     * @return Face number
     */
    int DetectFace(Orientation orient);
};

}; // Namespace end ------------------