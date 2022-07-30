#include "imu.hpp"
#include "logger.hpp"
#include "kalmanfilter.hpp"
#include <cmath>
#include "dateTime.hpp"

using namespace IMU;

//temporarily global - used instead of flash memory entry
std::array<Orientation, Imu::cubeFaces> calibrationPos;

void IMU::ImuTask(void *pvParameters) {
    Imu imu;

    vTaskDelay(ConvertToTicks(100ms));

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

        //check if new pos fits registered positions (from calibration) and if so send(somewhere cube face no.)

        if(Clock::now() - time > Imu::rollCooldown && newPos) {
            Logger::LOGI("IMU", __func__, ":", __LINE__, "New position");

            //todo check if cube is lying on any of detected positions
            newPos = false;
        }

        Logger::LOGI(ConvertToTicks(1s));

        vTaskDelay(Imu::taskPeriod);
    }
}

Imu::Imu() : MPU6050(Imu::pinScl, Imu::pinSda, Imu::port) {
    auto status = init();

    if(!status) {
            Logger::LOGE("IMU", __func__, ":", __LINE__, "Could not initalise IMU");
            //todo handle error
    }

    Logger::LOGI("IMU", __func__, ":", __LINE__, "Init");

    KALMAN pfilter(0.005);
    KALMAN rfilter(0.005);

    Logger::LOGI("IMU", __func__, ":", __LINE__, "IMU Checking calibration...");

    auto res = checkCalibration();

    if(!res) {
        Logger::LOGW("IMU", __func__, ":", __LINE__, "No calibration detected. Running calibration...");
        Calibrate();
    }
    Logger::LOGI("IMU", __func__, ":", __LINE__, "IMU Calibrated");

}

bool Imu::checkCalibration() {
    return isCalibrated;
}

void Imu::Calibrate() {
    Logger::LOGI("IMU", __func__, ":", __LINE__, "Total positions to be saved: ", Imu::cubeFaces);
    vTaskDelay(ConvertToTicks(500ms));

    for(auto i = 0; i < Imu::cubeFaces; i++) {
        Logger::LOGI("IMU", __func__, ":", __LINE__, "Saving position in 5s...");
        vTaskDelay(ConvertToTicks(5s));
        calibrationPos[i] = GetPosition();

        Logger::LOGI("IMU", __func__, ":", __LINE__, "Position saved: ");
        for(auto j = 0; j < calibrationPos[i].pos.size(); j++ ) {
            Logger::LOGI("IMU", __func__, ":", __LINE__, calibrationPos[i].pos[j]);
        }
    }

    isCalibrated = true;
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

