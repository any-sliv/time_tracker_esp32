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
QueueHandle_t ImuPositionQueue = xQueueCreate(1, sizeof(IMU::PositionQueueType));
QueueHandle_t ImuPositionGetQueue = xQueueCreate(1, sizeof(uint8_t));
QueueHandle_t ImuCalibrationInitQueue = xQueueCreate(1, sizeof(uint8_t));
QueueHandle_t ImuCalibrationStateQueue = xQueueCreate(1, sizeof("x,xx"));
QueueHandle_t SleepPauseQueue = xQueueCreate(1,16);
QueueHandle_t SleepStartQueue = xQueueCreate(1, sizeof(uint8_t));

namespace APP {

RTC_DATA_ATTR Timestamp lastBle;

static void sleep(std::chrono::duration<long long, std::micro> duration) {
    // Wake up after...
    esp_sleep_enable_timer_wakeup(duration.count());
    
    // Tasks to do before going to deep sleep!

    rtc_gpio_isolate(GPIO_NUM_2);
    rtc_gpio_isolate(GPIO_NUM_12); //TODO idk if it affects anything. brought it cuz of esp-idf reference recommendation
    // gpio_reset_pin(GPIO_NUM_0);
    // gpio_reset_pin(GPIO_NUM_2);
    // gpio_reset_pin(GPIO_NUM_4);
    // gpio_reset_pin(GPIO_NUM_12);
    // gpio_reset_pin(GPIO_NUM_13);
    // gpio_reset_pin(GPIO_NUM_14);
    // gpio_reset_pin(GPIO_NUM_15);
    // gpio_reset_pin(GPIO_NUM_25);
    // gpio_reset_pin(GPIO_NUM_26);
    // gpio_reset_pin(GPIO_NUM_27);
    // gpio_reset_pin(GPIO_NUM_32);
    // gpio_reset_pin(GPIO_NUM_33);
    // gpio_reset_pin(GPIO_NUM_34);
    // gpio_reset_pin(GPIO_NUM_35);
    // gpio_reset_pin(GPIO_NUM_36);
    // gpio_reset_pin(GPIO_NUM_37);
    // gpio_reset_pin(GPIO_NUM_38);
    // gpio_reset_pin(GPIO_NUM_39);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC8M, ESP_PD_OPTION_ON);


    if(BLE::Ble::state == BLE::Ble::ConnectionState::CONNECTED) {
        BLEDevice::deinit();
    }

    //TODO Firebeetle board deep sleep consumption: 1.6mA. Huge, spent a day trying to fix. 
    //TODO No signs of possible improvements so far :(
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
    // Sleep cooldown is a point in time
    Timestamp sleepCooldown = Clock::now() + 200ms;

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
        if(xQueueReceive(ImuReadyQueue, &imuReady, 0)) {
            vTaskResume(bleTask);
            // vTaskResume(batteryTask);
            sleepCooldown += 5s;
        }

        // Anyone delaying the sleep?
        char msg[16] = {0};
        if(xQueueReceive(SleepPauseQueue, msg, 0)) {
            std::string message(msg);
            ESP_LOGI(__FILE__, "%s:%d. Sleep deferred", __func__ ,__LINE__);
            if(message == "imuCalibration") {
                sleepCooldown = Clock::now() + 1min;
            }
            else if(message == "imuSendPosition") {
                sleepCooldown = Clock::now() + 1s;
            }
        }

        // Someone requested immediate sleep
        auto sleepStart = 0;
        if(xQueueReceive(SleepStartQueue, &sleepStart, 0)) {
            sleepCooldown = Clock::now();
        }

        // Is it the time to sleep?
        // WILL SLEEP IMMEDIATELY IF SYSTEM TIME WAS UPDATED!
        //TODO fix it? ^
        if(Clock::now() >= sleepCooldown) {
            sleep(20s);
        }
        TaskDelay(10ms);
    }
}

}; // namespace end ------------