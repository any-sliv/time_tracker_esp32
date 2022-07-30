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

    //read from flash
    // if no positions registered do calibration

    for(;;) {
        Orientation orient = imu.GetPosition();

        if(!(orient == oldPos)) {
            oldPos = imu.GetPosition();
            time = Clock::now();
            newPos = true;
        }

        if(Clock::now() - time > Imu::rollCooldown && newPos) {
            Logger::LOGI("New position");
            newPos = false;
        }

        Logger::LOGI(ConvertToTicks(1s));

        vTaskDelay(Imu::taskPeriod);
    }
}

Imu::Imu() : MPU6050(Imu::pinScl, Imu::pinSda, Imu::port) {
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

