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
QueueHandle_t ImuReadyQueue = xQueueCreate(1, sizeof(uint8_t));
QueueHandle_t BatteryQueue = xQueueCreate(1, sizeof(float));
QueueHandle_t ImuPositionQueue = xQueueCreate(10, sizeof(IMU::PositionQueueType));
QueueHandle_t ImuCalibrationInitQueue = xQueueCreate(1, 1);
QueueHandle_t ImuCalibrationStateQueue = xQueueCreate(1, 1);
TaskHandle_t pwrTask;

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

    //todo this is probably unsafe, but it crashed when ble is not initialized before
    // BLEDevice::deinit();

    //todo Firebeetle board deep sleep consumption: 1.6mA. Huge, spent a day trying to fix. No signs of possible improvements so far :(
    esp_deep_sleep_start();

    // Remember - after deep sleep whole application CPU will run application from the start
    // No memory is retained (except: flash and RTC_DATA_ATTR)
}

void AppManagementTask(void *pvParameters) {
    xTaskHandle imuTask;
    xTaskHandle bleTask;
    xTaskHandle batteryTask;

    auto res = xTaskCreate(IMU::ImuTask, "ImuTask", TASK_STACK_DEPTH_NORMAL, &imuTask, 
                                        TASK_PRIORITY_NORMAL, &imuTask);
    configASSERT(res);
    // vTaskSuspend(imuTask);

    res = xTaskCreate(BATTERY::BatteryTask, "BatteryTask", TASK_STACK_DEPTH_NORMAL, NULL, 
                                        TASK_PRIORITY_NORMAL, &batteryTask);
    configASSERT(res);
    // vTaskSuspend(batteryTask);

    res = xTaskCreatePinnedToCore(BLE::BleTask, "BleTask", TASK_STACK_DEPTH_MORE, NULL, 
                                        TASK_PRIORITY_NORMAL, &bleTask, /*core 0*/ 0);
    configASSERT(res);
    // vTaskSuspend(bleTask);

    //todo test how long does it take to connect from app since advertise
    //todo in an random scenario

    //todo check battery level. if below 20% sleep indefinetely/very loong

    //todo dont wakeup BLE until position has stabilized! imagine scenario when cube is in transport

    TaskDelay(10ms);

    for(;;) {
        // if(BLE::Ble::state == BLE::Ble::ConnectionState::CONNECTED) {
        //     lastBle = Clock::now();
        // }
        // if(Clock::now() - lastBle > 5min) {
        //     lastBle = Clock::now();
        //     vTaskResume(bleTask);
        //     TaskDelay(5s);
        // }

        // // Give imu time to process its data
        // TaskDelay(20ms);

        // auto item = 0;
        // if(xQueueReceive(ImuReadyQueue, &item, 0) == pdTRUE) {
        //     if(item == 1) {
        //         vTaskResume(bleTask);
        //         vTaskResume(batteryTask);
        //         TaskDelay(5s);
        //     }
        // }

        //todo read from BLE if client is done; if so - sleep

        TaskDelay(1s);
    }
}

}; // namespace end ------------