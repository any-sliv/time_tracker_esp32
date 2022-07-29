#include "imu.hpp"
#include "logger.hpp"
#include "kalmanfilter.hpp"
#include <cmath>
#include "dateTime.hpp"

using namespace IMU;

void IMU::ImuTask(void *pvParameters) {
    Imu imu;

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    Orientation oldPos = imu.GetPosition();
    Timestamp time = Clock::now();
    bool newPos = false;

    for(;;) {
        Orientation orient = imu.GetPosition();

        if(!(orient == oldPos)) {
            oldPos = imu.GetPosition();
            time = Clock::now();
            newPos = true;
        }

        if(Clock::now() - time > 5s && newPos) {
            Logger::LOGI("New position");
            newPos = false;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

Imu::Imu() : MPU6050(pinScl, pinSda, port) {
    auto status = init();

    if(!status) {
        Logger::LOGE("Cannot initalise IMU");
    }

    Logger::LOGI("IMU init");

    KALMAN pfilter(0.005);
    KALMAN rfilter(0.005);

    Logger::LOGI("IMU Checking calibration...");
    auto res = checkCalibration();

    if(!res) {
        Logger::LOGI("IMU no calibration. Initiating.");
        Calibrate();
    }
    Logger::LOGI("IMU calibrated.");
}

bool Imu::checkCalibration() {
    return 0;
}

void Imu::Calibrate() {

}

Orientation Imu::GetPosition() {
    //todo dont know if its safe
    //careful if adding something new to orientation structure

    //todo filtering?
    Orientation orient = {-getAccX(), -getAccY(), -getAccZ()};
    return orient;
}

int Imu::DetectFace(Orientation orient) {
    return 0;
}

