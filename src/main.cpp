/**
 * @file main.cpp
 * @author Maciej Sliwinski
 * @brief This file is a part of time_tracker_esp32 project.
 *
 * The code is distributed under the MIT License.
 * See the LICENCE file for more details.
 */


#include "appManagement.hpp"
#include "nvs.hpp"

extern "C" {
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
    #include <esp_system.h>
    #include <sdkconfig.h>
    #include "esp_ota_ops.h"
} // extern C close

extern "C" void app_main() {
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    ESP_LOGI(__FILE__, "%s:%d. Boot on partition: %s", __func__ ,__LINE__, running_partition->label);

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
