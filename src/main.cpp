/*****************************************************************************
 * @file main.cpp
 *
 * @brief Time tracker - ESP32
 *
 * @author Maciej Sliwinski
 * @date 2022
 * @version v1.0
 ****************************************************************************/

#include <iostream>
#include "gpio.hpp"
#include "wifi.hpp"
#include "logger.hpp"
//todo check cmakelists before includes modules

extern "C" {
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
    #include <esp_system.h>
    #include <sdkconfig.h>
} // extern C close

#define TASK_STACK_DEPTH_NORMAL (4U * 1024U)
#define TASK_PRIORITY_NORMAL (3U)

static void vTask1(void *pvParameters);
static void vTask2(void *pvParameters);

void tasksInit() {
    BaseType_t res = pdFAIL;
    res = xTaskCreate(vTask1, "Task1", TASK_STACK_DEPTH_NORMAL, NULL,
                                        TASK_PRIORITY_NORMAL, NULL);
    configASSERT(res);

    res = xTaskCreate(vTask2, "Task2", TASK_STACK_DEPTH_NORMAL, NULL, 
                                        TASK_PRIORITY_NORMAL, NULL);
    configASSERT(res);
}

static void vTask1(void *pvParameters) {
    //task vars
    WIFI::Wifi wifi;

    wifi.init();
    
    wifi.begin();
    for(;;) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void vTask2(void *pvParameters) {
    //task vars
    for(;;) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

extern "C" void app_main() {
    Logger::LOGI("Creating default event loop");
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    Logger::LOGI("Initializing NVS");
    ESP_ERROR_CHECK(nvs_flash_init());

    tasksInit();
    // Idle task
    while (1) {
        vTaskDelay(10 / portTICK_PERIOD_MS);  // keep it, otherwise watchdog is not fed
    }
}


/* --------------------------------------- END OF FILE ------------------------------------------- */
