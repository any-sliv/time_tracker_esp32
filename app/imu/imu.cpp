#include "imu.hpp"
#include "kalmanfilter.hpp"
#include <cmath>
#include <array>
#include "dateTime.hpp"

extern "C" {
    #include "esp_log.h"
    #include "freertos/FreeRTOS.h"
    #include "freertos/queue.h"
    #include "esp_sleep.h"
} // extern C close

// IMU stands for Inertial Measurement unit. 
// In this example MPU6050 is an IMU, so that naming is used interchangeably.

using namespace IMU;

// RTOS queues
extern QueueHandle_t ImuReadyQueue;
extern QueueHandle_t ImuPositionQueue;
extern QueueHandle_t ImuCalibrationInitQueue;
extern QueueHandle_t ImuCalibrationStateQueue;
extern QueueHandle_t SleepPauseQueue;

// This data will be stored in case of deep sleep. And buffer can hold large number of
// data in case of bluetooth connection lost. At reconnection will be sent.
// Its size is determined in vector implementation.
// Careful with size of RTC_DATA_ATTR. RTC memory has only 8kB
RTC_DATA_ATTR SimpleVector<PositionQueueType> SavedPositions;
RTC_DATA_ATTR Orientation oldPos;
RTC_DATA_ATTR Timestamp cooldown;
RTC_DATA_ATTR bool isNewPos;

void IMU::ImuTask(void *pvParameters) {
    ESP_LOGI(__FILE__, "%s:%d. Task init", __func__ ,__LINE__);

    Gpio imuGnd(22, Gpio::Mode::OUTPUT);
    imuGnd.Reset();
    Imu imu;
    oldPos = imu.GetPositionRaw();

    for(;;) {
        Orientation orient = imu.GetPositionRaw();

        if(!(oldPos == orient)) {
            isNewPos = true;
            oldPos = orient;
            // Block OnPositionChange until cube is steady / user has flipped completely
            cooldown = Clock::now();
        }

        if(Clock::now() - cooldown > Imu::rollCooldown && isNewPos) {
            // New position detected.
            imu.OnPositionChange(orient);

            auto item = 1;
            // todo Delay sleep??? maybe delete it?
            xQueueSend(ImuReadyQueue, &item, 0);

            isNewPos = false;
        }

        // Send batched data from vector to BLE
        while(uxQueueSpacesAvailable(ImuPositionQueue) > 0 && SavedPositions.GetActiveItems() > 0) {
            // Initiate send only if there are items available
            ESP_LOGI(__FILE__, "%s:%d. Active items: %d", __func__ ,__LINE__, SavedPositions.GetActiveItems());
            // Pop item from vector
            auto item = SavedPositions.Pop();
            xQueueSend(ImuPositionQueue, &item, 0);
            // This while cannot lock itself. If queue is full while condition is not met
        }

        // If BLE initiated calibration...
        auto val = 0;
        if(xQueueReceive(ImuCalibrationInitQueue, &val, 0) == pdTRUE) {
            if(val != 0) {
                auto ret = imu.CalibrateCubeFaces();
                xQueueSend(ImuCalibrationStateQueue, &ret, 0);
            }
        }

        TaskDelay(Imu::taskPeriod);
    }
}

Imu::Imu() : MPU6050(Imu::pinScl, Imu::pinSda, Imu::port) {
    ESP_LOGI(__FILE__, "%s:%d. Init", __func__ ,__LINE__);

    if(nvs.init() != ESP_OK) {
        ESP_LOGE(__FILE__, "%s:%d. Could not initialise NVS. Task suspended", __func__ ,__LINE__);
        vTaskSuspend(NULL);
    }

    // Init MPU6050
    // Imu operates in low power mode. Gyroscope is disabled after initalisation.
    if(! init()) {
        ESP_LOGE(__FILE__, "%s:%d. Could not initialise IMU. Task suspended", __func__ ,__LINE__);
        vTaskSuspend(NULL);
    }
    ESP_LOGI(__FILE__, "%s:%d. MPU6050 init done", __func__ ,__LINE__);

    KALMAN pfilter(0.005);
    KALMAN rfilter(0.005);

    ESP_LOGI(__FILE__, "%s:%d. Checking calibration...", __func__ ,__LINE__);

    auto res = checkCalibration();
    if(!res) {
        ESP_LOGE(__FILE__, "%s:%d. Calibration missing", __func__ ,__LINE__);
    }
}

int Imu::CalibrateCubeFaces() {
    //todo if calibration was interrupted, calibration is still '0' and process continues from middle instead of
    //todo beginning with erasing
    int cal = 0;
    auto nvsReadStatus = nvs.get("calibration", cal);
    //todo simplify with 'goto'?
    if(nvsReadStatus == ESP_OK) {
        if(cal == 1) {
            ESP_LOGW(__FILE__, "%s:%d. New calibration initiated. Erasing calibration.", __func__ ,__LINE__);
            // Erase positions from flash
            if(!(eraseCalibration())) {
                ESP_LOGE(__FILE__, "%s:%d. Could not erase flash", __func__ ,__LINE__);
                return CalibrationStatus::ERROR;
            } 
            // Set calibration to '0' - no calibration.
            if(nvs.set("calibration", 0) != ESP_OK) {
                ESP_LOGE(__FILE__, "%s:%d. Could not save flash", __func__ ,__LINE__);
                return CalibrationStatus::ERROR;
            }
        }
        else {
            ESP_LOGI(__FILE__, "%s:%d. 1", __func__ ,__LINE__);
        }
        //else; cal == 0 - everything's fine. go do the stuff
    } 
    else if(nvsReadStatus == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(__FILE__, "%s:%d. 3", __func__ ,__LINE__);
        if(nvs.set("calibration", 0) != ESP_OK) {
            ESP_LOGE(__FILE__, "%s:%d. Could not save flash", __func__ ,__LINE__);
            return CalibrationStatus::ERROR;
        }
        ESP_LOGI(__FILE__, "%s:%d. 2", __func__ ,__LINE__);
    }
    else {
        ESP_LOGE(__FILE__, "%s:%d. Could not read from flash", __func__ ,__LINE__);
        return CalibrationStatus::ERROR;
    }

    ESP_LOGI(__FILE__, "%s:%d. Calibrating new position", __func__ ,__LINE__);
    // Let the cube stabilize
    TaskDelay(200ms);
    auto positionToSave = GetPositionRaw();
    positionToSave.PrintPosition();

    if(checkCalibration(positionToSave)) {
        // If this position exists in memory
        ESP_LOGW(__FILE__, "%s:%d. Position already exists in memory", __func__ ,__LINE__);
        return CalibrationStatus::ALREADY_EXISTS;
    }

    auto positions = getNoOfCalibratedPositions();
    std::string nvsEntry{"pos"};
    nvsEntry += std::to_string(positions);

    //todo check if you can save whole orientation object
    // Saving in flash using pos<faceNumber> key
    auto ret = nvs.set(nvsEntry.c_str(), positionToSave.pos);
    if(ret != ESP_OK) {
        ESP_LOGE(__FILE__, "%s:%d. Could not save flash", __func__ ,__LINE__);
        return CalibrationStatus::ERROR;
    }

    if(positions >= Imu::cubeFaces) {
        // All possible positions should exist in memory
        ESP_LOGI(__FILE__, "%s:%d. Calibration done", __func__ ,__LINE__);
        // Additional check just to be sure
        if(checkCalibration()) {
            if(nvs.set("calibration", 1) != ESP_OK) {
                return CalibrationStatus::ERROR;
            }
            return CalibrationStatus::DONE;
        }
        else {
            return CalibrationStatus::ERROR;
        }
    }
    return CalibrationStatus::OK;
}

Orientation Imu::GetPositionRaw() {
    return Orientation(-getAccX(), -getAccY(), -getAccZ());
}

void Imu::OnPositionChange(const Orientation& newOrient) {
    // Each write in RTC_DATA_ATTR SavedPositions is "saving". This data is retained during sleep.
    ESP_LOGI(__FILE__, "%s:%d. New position", __func__ ,__LINE__);
    
    if(SavedPositions.GetActiveItems() >= SavedPositions.size()) {
        ESP_LOGW(__FILE__, "%s:%d. Could not save position. RTC Memory array full.", __func__ ,__LINE__);
        return;
    }

    auto face = DetectFace(newOrient);

    if(face == -1) {
        ESP_LOGW(__FILE__, "%s:%d. Wrong position detected. Not any of calibrated positions", __func__ ,__LINE__);
        return;
    }
    //todo reject from saving positions which has the same position as previous
    // Item will hold values only of those two parameters
    PositionQueueType item(face, time(NULL));
    // Save face and system time
    SavedPositions.Push(item);
}

int Imu::getNoOfCalibratedPositions() {
    // Iterate faces from 1. They are a real entity
    int faces = 1;
    for (auto i = 1; i <= Imu::cubeFaces; i++) {
        std::string nvsEntry{"pos"};
        nvsEntry += std::to_string(i);

        Orientation tmp;
        //Return false if flash hasn't no. of positions == cubeFaces
        if(nvs.get(nvsEntry.c_str(), tmp.pos) != ESP_OK) {
            break;
        }
        faces++;
    }
    return faces;
}

bool Imu::checkCalibration() {
    for (auto i = 1; i <= Imu::cubeFaces; i++) {
        std::string nvsEntry{"pos"};
        nvsEntry += std::to_string(i);

        Orientation tmp;
        //Return false if flash hasn't no. of positions == cubeFaces
        auto res = nvs.get(nvsEntry.c_str(), tmp.pos);
        tmp.PrintPosition();
        if(res != ESP_OK) {
            return false;
        }
    }
    return true;
}

bool Imu::checkCalibration(const Orientation& orient) {
    for (auto i = 1; i <= Imu::cubeFaces; i++) {
        std::string nvsEntry{"pos"};
        nvsEntry += std::to_string(i);

        Orientation tmp;
        if(nvs.get(nvsEntry.c_str(), tmp.pos) != ESP_OK) {
            return false;
        }
        // If given position exists in flash return false
        if(tmp == orient) return false;
    }
    return true;
}

bool Imu::eraseCalibration() {
    for (auto i = 1; i <= Imu::cubeFaces + 10; i++) {
        std::string nvsEntry{"pos"};
        nvsEntry += std::to_string(i);
        auto res = nvs.erase(nvsEntry.c_str());
        if(!(res == ESP_OK || (res == ESP_ERR_NVS_NOT_FOUND))) {
            // Res different than ESP_OK or not_found
            return false;
        }
    }
    return true;
}

int Imu::DetectFace(const Orientation& orient) {
    // Faces start from 1, they're a real thing. Face = 0 -> empty entry
    for(auto i = 1; i <= Imu::cubeFaces; i++) {
        std::string nvsEntry{"pos"};
        nvsEntry += std::to_string(i);
        // Read from flash using pos<faceNumber> key
        Orientation savedPos;
        auto ret = nvs.get(nvsEntry.c_str(), savedPos.pos);
        if(ret != ESP_OK) {
            ESP_LOGE(__FILE__, "%s:%d. Could not read flash", __func__ ,__LINE__);
        }

        if(savedPos == orient) {
            return i;
        }
    }
    return -1;
}
