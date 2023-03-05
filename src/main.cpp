/*****************************************************************************
 * @file main.cpp
 *
 * @brief Time tracker - ESP32
 *
 * @author Maciej Sliwinski
 * @date 2022
 * @version v1.0
 ****************************************************************************/

#include "appManagement.hpp"
#include "nvs.hpp"

extern "C" {
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
    #include <esp_system.h>
    #include <sdkconfig.h>
} // extern C close

extern "C" void app_main() {
    BaseType_t res = pdFAIL;

    res = xTaskCreatePinnedToCore(APP::AppManagementTask, "AppManagementTask", TASK_STACK_DEPTH_NORMAL, NULL, 
                                        TASK_PRIORITY_NORMAL, NULL, 0);
    configASSERT(res);
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    NVS::Nvs nvs;
    ESP_ERROR_CHECK(nvs.init());

    while (1) {
        vTaskDelay(10 / portTICK_PERIOD_MS);  // keep it, otherwise watchdog is not fed
    }
}


/* --------------------------------------- END OF FILE ------------------------------------------- */
