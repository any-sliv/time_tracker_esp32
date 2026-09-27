/**
 * @file imu.cpp
 * @author Maciej Sliwinski
 * @brief This file is a part of time_tracker_esp32 project.
 *
 * The code is distributed under the MIT License.
 * See the LICENCE file for more details.
 */

#include "imu.hpp"
#include "kalmanfilter.hpp"
#include <cmath>
#include <array>
#include <string>
#include "dateTime.hpp"
#include "sleepPause.hpp"

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
extern QueueHandle_t ImuPositionGetQueue;
extern QueueHandle_t ImuCalibrationInitQueue;
extern QueueHandle_t ImuCalibrationStateQueue;
extern QueueHandle_t SleepPauseQueue;

// This data will be stored in case of deep sleep. And buffer can hold large number of
// data in case of bluetooth connection lost. At reconnection will be sent.
// Its size is determined in vector implementation.
// Careful about size with RTC_DATA_ATTR (must be global). RTC memory has only 8kB. 

// Stores data for application <face,startTime>
RTC_DATA_ATTR SimpleVector<PositionQueueType, 100> SavedPositions;
// Used for calibration
RTC_DATA_ATTR SimpleVector<Orientation, Imu::cubeFaces> CalibrationBank;
// Imu logic
RTC_DATA_ATTR Orientation oldPos;
RTC_DATA_ATTR Timestamp cooldown;
RTC_DATA_ATTR bool isNewPos;

void IMU::ImuTask(void *pvParameters) {
    ESP_LOGI(__FILE__, "%s:%d. Task init", __func__ ,__LINE__);
    
    Imu imu;
    
    for(;;) {
        Orientation orient = imu.GetPositionRaw();

        if(!(oldPos == orient)) {
            isNewPos = true;
            oldPos = orient;
            // Block OnPositionChange until cube is steady / user has flipped completely
            cooldown = Clock::now();
        }

        if(Clock::now() - cooldown > Imu::rollCooldown && isNewPos) {
            // New position detected
            if(imu.OnPositionChange(orient)) {
                // New position accepted
                auto item = 1;
                xQueueSend(ImuReadyQueue, &item, 0);
            }
            isNewPos = false;
        }

        uint16_t budget = 0;
        // Send batched data from vector to BLE
        if(xQueueReceive(ImuPositionGetQueue, &budget, 0)) {
            // Initiate send only if there are items available
            if(SavedPositions.GetActiveItems()) {
                // Buffer must be exactly the queue item size - xQueueSend copies all of it
                char batch[PositionBatchMaxLen] = {0};
                std::string out;

                // PopOldest, not Pop: Pop is LIFO, which would hand BLE the newest
                // records first and put the drain out of chronological order.
                while(SavedPositions.GetActiveItems()) {
                    // Peek and format before committing - a record that does not fit
                    // must stay in the vector for the next read.
                    PositionQueueType next = SavedPositions.PeekOldest();
                    std::string record = std::to_string(next.startTime) + "," +
                                         std::to_string(next.face);

                    // Record length is NOT fixed: face is unsigned, so an unmatched
                    // face (-1) serialises as 4294967295 and the record is ~21 bytes.
                    const size_t addition = out.empty() ? record.size() : record.size() + 1;

                    if(!out.empty() && out.size() + addition > budget) {
                        break;
                    }
                    if(out.empty() && record.size() > budget) {
                        // Emit it anyway. At an unnegotiated MTU (23) the budget is 20
                        // bytes and a 21-byte record would never fit, stalling the drain
                        // forever. NimBLE serves oversized values via read-blob.
                        ESP_LOGW(__FILE__, "%s:%d. Record %u B exceeds MTU budget %u B, sending anyway",
                                 __func__ ,__LINE__, (unsigned) record.size(), (unsigned) budget);
                    }

                    if(!out.empty()) {
                        out += ';';
                    }
                    out += record;
                    SavedPositions.DropOldest();

                    if(out.size() >= PositionBatchMaxLen - 1) {
                        break;
                    }
                }

                strncpy(batch, out.c_str(), PositionBatchMaxLen - 1);
                xQueueSend(ImuPositionQueue, batch, 0);

                // Defer sleep. Let someone process the data
                auto reason = SleepPauseReason::ImuSendPosition;
                xQueueSend(SleepPauseQueue, &reason, 0);
            }
        }

        // If BLE initiated calibration...
        auto val = 0;
        if(xQueueReceive(ImuCalibrationInitQueue, &val, 0)) {
            if(val != 0) {
                auto pos = -1;
                // Pos will have face number
                auto ret = imu.CalibrateCubeFaces(pos);
                // Create a string "x,xx" containing calibration status and calibrated face
                std::string calibrationStatus = std::to_string(ret) + ',' + std::to_string(pos);
                xQueueSend(ImuCalibrationStateQueue, calibrationStatus.data(), 0);
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

    auto res = checkCalibration();
    if(!res) {
        ESP_LOGE(__FILE__, "%s:%d. Calibration missing", __func__ ,__LINE__);
    }
}

int Imu::CalibrateCubeFaces(int& returnPosition) {
    //todo calibration cancel option!
    ESP_LOGI(__FILE__, "%s:%d. Calibrating new position", __func__ ,__LINE__);
    // Let the cube stabilize
    TaskDelay(200ms);
    auto positionToSave = GetPositionRaw();

    if(CalibrationBank.isDuplicate(positionToSave)) {
        ESP_LOGW(__FILE__, "%s:%d. Position already exists", __func__ ,__LINE__);
        return CalibrationStatus::ALREADY_EXISTS;
    }

    // Save temporarily
    CalibrationBank.Push(positionToSave);

    auto positions = CalibrationBank.GetActiveItems();
    returnPosition = positions;

    if(positions >= Imu::cubeFaces) {
        // All positions registered. Erase previous and save current
        auto err = eraseCalibration();
        if(err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(__FILE__, "%s:%d. Could not erase flash %s", __func__ ,__LINE__, esp_err_to_name(err));
            return CalibrationStatus::ERROR;
        }

        for(int i = positions; i >= 1; i--) {
            auto value = CalibrationBank.Pop();
            std::string nvsKey{"pos"};
            nvsKey += std::to_string(i);

            // Saving in flash using pos<faceNumber> key
            auto ret = nvs.set(nvsKey.c_str(), value);
            if(ret != ESP_OK) {
                ESP_LOGE(__FILE__, "%s:%d. Could not save flash %s", __func__ ,__LINE__, esp_err_to_name(ret));
                return CalibrationStatus::ERROR;
            }
        }

        // Additional check just to be sure
        if(checkCalibration()) {
            ESP_LOGI(__FILE__, "%s:%d. Calibration done", __func__ ,__LINE__);
            return CalibrationStatus::DONE;
        }
        return CalibrationStatus::ERROR;
    }
    
    return CalibrationStatus::OK;
}

Orientation Imu::GetPositionRaw() {
    return Orientation(-getAccX(), -getAccY(), -getAccZ());
}

bool Imu::OnPositionChange(const Orientation& newOrient) {
    // Each write in RTC_DATA_ATTR SavedPositions is "saving". This data is retained during sleep/wake cycles.

    if(SavedPositions.GetActiveItems() >= SavedPositions.Size()) {
        ESP_LOGW(__FILE__, "%s:%d. Could not save position. RTC Memory array full. Items: %d", __func__ ,__LINE__, SavedPositions.GetActiveItems());
        return false;
    }

    auto face = DetectFace(newOrient);

    if(face == -1) {
        ESP_LOGW(__FILE__, "%s:%d. Wrong position detected. Not any of calibrated positions", __func__ ,__LINE__);
    }

    // Skip if no positions exist in memory
    if(SavedPositions.GetActiveItems()) {
        PositionQueueType prevPos = SavedPositions.Pop();
        // Put back item we popped
        SavedPositions.Push(prevPos);
        if(prevPos.face == face) {
            // Same position as before, dont save it
            return false;
        }
    }

    // Item will hold values only of those two parameters
    PositionQueueType item(face, time(NULL));
    // Save face and system time
    SavedPositions.Push(item);
    ESP_LOGI(__FILE__, "%s:%d. New position", __func__ ,__LINE__);
    
    return true;
}

int Imu::getNoOfCalibratedPositions() {
    // Iterate faces from 1. They are a real entity
    int faces = 1;
    for (auto i = 1; i <= Imu::cubeFaces; i++) {
        std::string nvsEntry{"pos"};
        nvsEntry += std::to_string(i);

        Orientation tmp;
        //Return false if flash hasn't no. of positions == cubeFaces
        if(nvs.get(nvsEntry.c_str(), tmp) != ESP_OK) {
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
        auto res = nvs.get(nvsEntry.c_str(), tmp);
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
        if(nvs.get(nvsEntry.c_str(), tmp) != ESP_OK) {
            return false;
        }
        // If given position exists in flash return false
        if(tmp == orient) return false;
    }
    return true;
}

esp_err_t Imu::eraseCalibration() {
    esp_err_t err = ESP_FAIL;
    // '+10' search for more entries, if they exist - will be wiped
    for (auto i = 1; i <= Imu::cubeFaces + 10; i++) {
        std::string nvsEntry{"pos"};
        nvsEntry += std::to_string(i);
        err = nvs.erase(nvsEntry.c_str());
        if(err != ESP_OK && err != ESP_ERR_NVS_INVALID_HANDLE) {
            // Res different than ESP_OK or not_found
            return err;
        }
    }
    return err;
}

int Imu::DetectFace(const Orientation& orient) {
    // Faces start from 1, they're a real thing.
    for(auto i = 1; i <= Imu::cubeFaces; i++) {
        std::string nvsEntry{"pos"};
        nvsEntry += std::to_string(i);
        // Read from flash using pos<faceNumber> key
        Orientation savedPos(0,0,0);
        auto ret = nvs.get(nvsEntry.c_str(), savedPos);
        if(ret != ESP_OK) {
            ESP_LOGE(__FILE__, "%s:%d. Could not read flash", __func__ ,__LINE__);
        }

        if(savedPos == orient) {
            return i;
        }
    }
    return -1;
}
