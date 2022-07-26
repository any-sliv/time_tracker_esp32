#include "imu.hpp"
#include "logger.hpp"
#include "kalmanfilter.hpp"
#include <cmath>

using namespace IMU;

void IMU::ImuTask(void *pvParameters) {
    Imu imu;
    //todo change IMU -> IMUPosition?

    float ax,ay,az,gx,gy,gz;
    float pitch, roll;
    float fpitch, froll;

    KALMAN pfilter(0.005);
    KALMAN rfilter(0.005);

    uint32_t lasttime = 0;
    int count = 0;

    for(;;) {
        // ax = -imu.sensor->getAccX();
        // ay = -imu.sensor->getAccY();
        // az = -imu.sensor->getAccZ();
        // gx = imu.sensor->getGyroX();
        // gy = imu.sensor->getGyroY();
        // gz = imu.sensor->getGyroZ();
        // pitch = atan(ax/az)*57.2958;
        // roll = atan(ay/az)*57.2958;
        // fpitch = pfilter.filter(pitch, gy);
        // froll = rfilter.filter(roll, -gx);
        // count++;
        // if(esp_log_timestamp() / 1000 != lasttime) {
        //     lasttime = esp_log_timestamp() / 1000;
        //     printf("Samples:%d ", count);
        //     count = 0;
        //     printf(" Acc:(%4.2f,%4.2f,%4.2f)", ax, ay, az);
        //     printf("Gyro:(%6.3f,%6.3f,%6.3f)", gx, gy, gz);
        //     printf(" Pitch:%6.3f ", pitch);
        //     printf(" Roll:%6.3f ", roll);
        //     printf(" FPitch:%6.3f ", fpitch);
        //     printf(" FRoll:%6.3f \n", froll);
        // }
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

Imu::Imu() : MPU6050(pinScl, pinSda, port) {
    auto status = init();

    if(!status) {
        Logger::LOGE("Cannot initalise IMU");
    }

    Logger::LOGI("IMU init");
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

int Imu::DetectFace(Orientation orient) {
    return 0;
}

