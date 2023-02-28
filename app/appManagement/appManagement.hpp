/*
 * powerManagement.hpp
 *
 *  Created on: Sep, 2022
 *      Author: macsli
 */

#include "imu.hpp"
#include "ble.hpp"
#include "battery.hpp"
#include "dateTime.hpp"

extern "C" {
    #include "driver/adc.h"
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_sleep.h"
    #include "soc/rtc.h"
    #include "driver/rtc_io.h"
} // extern C close

#define TASK_STACK_DEPTH_NORMAL (4U * 1024U)
#define TASK_STACK_DEPTH_MORE (6U * 1024U)
#define TASK_PRIORITY_NORMAL (3U)

//todo add some info about queues
QueueHandle_t BatteryQueue = xQueueCreate(1, sizeof(float));
QueueHandle_t ImuReadyQueue = xQueueCreate(1, sizeof(uint8_t));
QueueHandle_t ImuPositionQueue = xQueueCreate(10, sizeof(IMU::PositionQueueType));
QueueHandle_t ImuCalibrationInitQueue = xQueueCreate(1, 1);
QueueHandle_t ImuCalibrationStateQueue = xQueueCreate(1, 1);
QueueHandle_t SleepPauseQueue = xQueueCreate(1,16);
QueueHandle_t SleepStartQueue = xQueueCreate(1,1);

namespace APP {

RTC_DATA_ATTR Timestamp lastBle;

static void sleep(std::chrono::duration<long long, std::micro> duration) {
    // Wake up after...
    esp_sleep_enable_timer_wakeup(duration.count());
    
    // Tasks to do before going to deep sleep!

    // rtc_gpio_isolate(GPIO_NUM_2);
    // rtc_gpio_isolate(GPIO_NUM_12); //todo idk if it affects anything. brought it cuz of esp-idf reference recommendation
    gpio_reset_pin(GPIO_NUM_0);
    gpio_reset_pin(GPIO_NUM_2);
    gpio_reset_pin(GPIO_NUM_4);
    gpio_reset_pin(GPIO_NUM_12);
    gpio_reset_pin(GPIO_NUM_13);
    gpio_reset_pin(GPIO_NUM_14);
    gpio_reset_pin(GPIO_NUM_15);
    gpio_reset_pin(GPIO_NUM_25);
    gpio_reset_pin(GPIO_NUM_26);
    gpio_reset_pin(GPIO_NUM_27);
    gpio_reset_pin(GPIO_NUM_32);
    gpio_reset_pin(GPIO_NUM_33);
    gpio_reset_pin(GPIO_NUM_34);
    gpio_reset_pin(GPIO_NUM_35);
    gpio_reset_pin(GPIO_NUM_36);
    gpio_reset_pin(GPIO_NUM_37);
    gpio_reset_pin(GPIO_NUM_38);
    gpio_reset_pin(GPIO_NUM_39);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

    if(BLE::Ble::state == BLE::Ble::ConnectionState::CONNECTED) {
        //todo this is probably unsafe, but it crashed when ble is not initialized before
        BLEDevice::deinit();
    }

    //todo Firebeetle board deep sleep consumption: 1.6mA. Huge, spent a day trying to fix. 
    //todo No signs of possible improvements so far :(
    esp_deep_sleep_start();
    // Remember - after deep sleep whole application CPU will run application from the start
    // No memory is retained (except: flash and RTC_DATA_ATTR)
}

void AppManagementTask(void *pvParameters) {
    ESP_LOGI(__FILE__, "%s:%d. Task init", __func__ ,__LINE__);

    xTaskHandle imuTask;
    xTaskHandle bleTask;
    xTaskHandle batteryTask;

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
    // vTaskSuspend(bleTask);

    //todo test how long does it take to connect from app since advertise
    //todo in an random scenario

    //todo check battery level. if below 20% sleep indefinetely/very loong

    //todo dont wakeup BLE until position has stabilized! imagine scenario when cube is in transport

    TaskDelay(10ms);
    // Sleep cooldown is a point in time
    Timestamp sleepCooldown = Clock::now() + 30s; //todo DECREASE

    for(;;) {
        if(BLE::Ble::state == BLE::Ble::ConnectionState::CONNECTED) {
            lastBle = Clock::now();
        }
        // Last connection with BLE more than 5 min ago?
        if(Clock::now() - lastBle > 5min) {
            lastBle = Clock::now();
            vTaskResume(bleTask);
            // Let the BLE do the stuff
            sleepCooldown += 5s;
        }

        auto imuReady = 0;
        // IMU got new position?
        if(xQueueReceive(ImuReadyQueue, &imuReady, 0) == pdTRUE) {
            vTaskResume(bleTask);
            // vTaskResume(batteryTask);
            sleepCooldown += 5s;
        }

        // Anyone delaying the sleep?
        char msg[16] = {0};
        if(xQueueReceive(SleepPauseQueue, msg, 0) == pdTRUE) {
            std::string message(msg);
            if(message == "imuCalibration") {
                ESP_LOGI(__FILE__, "%s:%d. Sleep defer 1min!", __func__ ,__LINE__);
                sleepCooldown += 1min;
            }
        }

        // Someone requested immediate sleep
        auto sleepStart = 0;
        if(xQueueReceive(SleepStartQueue, &sleepStart, 0) == pdTRUE) {
            sleepCooldown = Clock::now();
        }

        // Is it the time to sleep?
        // WILL SLEEP IMMEDIATELY IF SYSTEM TIME WAS UPDATED?
        if(Clock::now() >= sleepCooldown) {
            sleep(15s);
        }
        TaskDelay(10ms);
    }
}

}; // namespace end ------------