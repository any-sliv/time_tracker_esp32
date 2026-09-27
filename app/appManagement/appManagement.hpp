/**
 * @file appManagement.hpp
 * @author Maciej Sliwinski
 * @brief This file is a part of time_tracker_esp32 project.
 *
 * The code is distributed under the MIT License.
 * See the LICENCE file for more details.
 */

#include "imu.hpp"
#include "ble.hpp"
#include "battery.hpp"
#include "dateTime.hpp"
#include "sleepPause.hpp"
#include <algorithm>

extern "C" {
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_sleep.h"
    #include "soc/rtc.h"
    #include "driver/rtc_io.h"
} // extern C close

#define TASK_STACK_DEPTH_NORMAL (4U * 1024U)
#define TASK_STACK_DEPTH_MORE (6U * 1024U)
#define TASK_PRIORITY_NORMAL (3U)

//TODO add some info about queues
//TODO simplify queues???
QueueHandle_t BatteryQueue = xQueueCreate(1, sizeof(int));
QueueHandle_t ImuReadyQueue = xQueueCreate(1, sizeof(uint8_t));
// Carries a formatted ';'-separated batch of records, not a single struct
QueueHandle_t ImuPositionQueue = xQueueCreate(1, IMU::PositionBatchMaxLen);
// Carries the byte budget (negotiated MTU - 3) the batch must fit into
QueueHandle_t ImuPositionGetQueue = xQueueCreate(1, sizeof(uint16_t));
QueueHandle_t ImuCalibrationInitQueue = xQueueCreate(1, sizeof(uint8_t));
QueueHandle_t ImuCalibrationStateQueue = xQueueCreate(1, sizeof("x,xx"));
QueueHandle_t SleepPauseQueue = xQueueCreate(1, sizeof(SleepPauseReason));
QueueHandle_t SleepStartQueue = xQueueCreate(1, sizeof(uint8_t));

namespace APP {

RTC_DATA_ATTR Timestamp lastBle;

/// Longest an established BLE connection may hold the device out of deep sleep.
static constexpr std::chrono::seconds connectionHold = 30s;

static void sleep(std::chrono::duration<long long, std::micro> duration) {
    // Wake up after...
    esp_sleep_enable_timer_wakeup(duration.count());
    
    // Tasks to do before going to deep sleep!

    rtc_gpio_isolate(GPIO_NUM_2);
    rtc_gpio_isolate(GPIO_NUM_12);

    if(BLE::Ble::state == BLE::Ble::ConnectionState::CONNECTED) {
        NimBLEDevice::deinit();
    }

    ESP_LOGI(__FILE__, "%s:%d. zzz...", __func__ ,__LINE__);
    esp_deep_sleep_start();
    // Remember - after deep sleep whole application CPU will run application from the start
    // No memory is retained (except: flash and RTC_DATA_ATTR)
}

void AppManagementTask(void *pvParameters) {
    ESP_LOGI(__FILE__, "%s:%d. Task init", __func__ ,__LINE__);

    TaskHandle_t imuTask;
    TaskHandle_t bleTask;
    TaskHandle_t batteryTask;

    auto res = xTaskCreate(IMU::ImuTask, "ImuTask", TASK_STACK_DEPTH_NORMAL, NULL, 
                                        TASK_PRIORITY_NORMAL, &imuTask);
    configASSERT(res);
    // vTaskSuspend(imuTask);

    res = xTaskCreate(BATTERY::BatteryTask, "BatteryTask", TASK_STACK_DEPTH_NORMAL, NULL, 
                                        TASK_PRIORITY_NORMAL, &batteryTask);
    configASSERT(res);
    // vTaskSuspend(batteryTask);

    res = xTaskCreate(BLE::BleTask, "BleTask", TASK_STACK_DEPTH_MORE, NULL, 
                                        TASK_PRIORITY_NORMAL, &bleTask);
    configASSERT(res);
    vTaskSuspend(bleTask);

    //TODO check battery level. if below 20% sleep indefinetely/very loong

    // If you want to debug device, see whats going on without it
    // going to sleep so quick all the time: increase this cooldown!
    // Sleep cooldown is a point in time.
    // MonoClock, not Clock: the BLE Current Time characteristic calls settimeofday(),
    // and a wall-clock deadline would jump into the past the instant a client sets
    // the clock, sleeping the device mid-session.
    MonoTimestamp sleepCooldown = MonoClock::now() + 1000ms;

    // How long an established BLE connection may hold the device awake. Kept separate
    // from sleepCooldown (rather than max()'d into it) so a client-requested sleep can
    // still take effect immediately. Plain locals are correct here: a connection cannot
    // survive deep sleep (NimBLEDevice::deinit() in sleep()), so resetting each boot is
    // exactly the semantics we want.
    MonoTimestamp connDeadline = MonoClock::now();
    auto prevState = BLE::Ble::state;

    for(;;) {
        const auto bleState = BLE::Ble::state;

        if(bleState == BLE::Ble::ConnectionState::CONNECTED) {
            lastBle = Clock::now();
            if(prevState != BLE::Ble::ConnectionState::CONNECTED) {
                // Rising edge: hold awake so a transfer is not cut off mid-drain
                connDeadline = MonoClock::now() + connectionHold;
                ESP_LOGI(__FILE__, "%s:%d. Connected, holding awake up to %llds",
                         __func__, __LINE__, (long long) connectionHold.count());
            }
        }
        prevState = bleState;

        // Last connection with BLE more than 5 min ago?
        if(Clock::now() - lastBle > 5min) {
            lastBle = Clock::now();
            vTaskResume(bleTask);
            // Let the BLE do the stuff
            sleepCooldown = std::max(sleepCooldown, MonoClock::now() + 5s);
        }

        auto imuReady = 0;
        // IMU got new position?
        if(xQueueReceive(ImuReadyQueue, &imuReady, 0)) {
            vTaskResume(bleTask);
            // vTaskResume(batteryTask);
            sleepCooldown = std::max(sleepCooldown, MonoClock::now() + 5s);
        }

        // Anyone delaying the sleep?
        // Always max() - assigning would let a 1s position deferral stomp the
        // 5min OTA window and abort a firmware update.
        SleepPauseReason reason;
        if(xQueueReceive(SleepPauseQueue, &reason, 0)) {
            ESP_LOGI(__FILE__, "%s:%d. Sleep deferred, reason %d", __func__ ,__LINE__, (int) reason);
            switch(reason) {
                case SleepPauseReason::ImuCalibration:
                    sleepCooldown = std::max(sleepCooldown, MonoClock::now() + 1min);
                    break;
                case SleepPauseReason::ImuSendPosition:
                    sleepCooldown = std::max(sleepCooldown, MonoClock::now() + 1s);
                    break;
                case SleepPauseReason::OtaUpdate:
                    sleepCooldown = std::max(sleepCooldown, MonoClock::now() + 5min);
                    break;
                case SleepPauseReason::TimeSet:
                    sleepCooldown = std::max(sleepCooldown, MonoClock::now() + 5s);
                    break;
            }
        }

        // Someone requested sleep. Graceful leaves a moment for the in-flight ATT
        // response to reach the client before the stack is torn down.
        SleepStartMode sleepStart;
        if(xQueueReceive(SleepStartQueue, &sleepStart, 0)) {
            if(sleepStart == SleepStartMode::Immediate) {
                sleepCooldown = MonoClock::now();
                connDeadline = MonoClock::now();
            }
            else {
                sleepCooldown = MonoClock::now() + 500ms;
                connDeadline = MonoClock::now();
            }
        }

        // Is it the time to sleep? An active connection holds us past sleepCooldown,
        // but only until connDeadline.
        const auto now = MonoClock::now();
        if(now >= sleepCooldown && now >= connDeadline) {
            sleep(20s);
        }
        TaskDelay(10ms);
    }
}

}; // namespace end ------------