/**
 * @file ble.cpp
 * @author Maciej Sliwinski
 * @brief This file is a part of time_tracker_esp32 project.
 *
 * The code is distributed under the MIT License.
 * See the LICENCE file for more details.
 */

#include "ble.hpp"
#include "battery.hpp"
#include "imu.hpp"
#include "NimBLEDevice.h"
#include "NimBLEUtils.h"
#include "NimBLEServer.h"

extern "C" {
    #include <freertos/FreeRTOS.h>
    #include "freertos/queue.h"
    #include "esp_bt.h"
    #include "string.h"
    #include "esp_log.h"
    #include "driver/rtc_io.h"
    #include "esp_sleep.h"
} // extern C close

using namespace BLE;

extern QueueHandle_t BatteryQueue;
extern QueueHandle_t ImuPositionQueue;
extern QueueHandle_t ImuPositionGetQueue;
extern QueueHandle_t ImuCalibrationInitQueue;
extern QueueHandle_t ImuCalibrationStateQueue;
extern QueueHandle_t SleepPauseQueue;
extern QueueHandle_t SleepStartQueue;

BLEServer * Ble::server = NULL;
esp_ota_handle_t Ble::otaHandle = NULL;
Ble::ConnectionState Ble::state = Ble::ConnectionState::IDLE;

const std::string advServiceUuid = "8227dcb2-30e3-11ed-a261-0242ac120002";
// <const> static constexpr char; <const> = adding it removes -Wwrite-strings warning (dunno why)
const static constexpr char * uuidImuPositionService = "7bef916a-3141-11ed-a261-0242ac120000";
const static constexpr char * uuidImuPositionCharateristic = "7bef916a-3141-11ed-a261-0242ac120001";
const static constexpr char * uuidImuCalibrationCharateristic = "7bef916a-3141-11ed-a261-0242ac120002";

const static constexpr char * uuidSleep = "646b8837-cea9-4006-be25-00c990029e90";

const static constexpr char * uuidDeviceFirmwareUpdateService = "00009921-1212-efde-1523-785feabcd123"; 
const static constexpr char * uuidDeviceFirmwareDataCharacteristic = "00009921-1212-efde-1523-785feabcd124"; 
const static constexpr char * uuidDeviceFirmwareControlCharacteristic = "00009921-1212-efde-1523-785feabcd125"; 

// Bluetooth® SIG specified services/chars below

// For one characteristic services: service uuid = characteristic uuid
const static constexpr char * uuidFirmwareRevision = "2A26";
const static constexpr char * uuidManufacturerName = "2A29"; 
const static constexpr char * uuidBattery = "2A19"; 
const static constexpr char * uuidCurrentTime = "2A2B";

void BLE::BleTask(void *pvParameters) {
    ESP_LOGI(__FILE__, "%s:%d. Task init", __func__ ,__LINE__);

    Ble ble;
    ble.Init();

    for(;;) {
        // Most of functionalities is done in BLE characteristic callbacks

        //TODO pairing process???

        //TODO register ESP_ERRORS and send them thru ble?

        TaskDelay(1s);
    }
}

void Ble::Init() {
    BLEDevice::init("Time tracker");

    // Scheme: Callback to characteristic. Characteristics to service. 

    // IMU Position service 
    BLE::Service positionService(uuidImuPositionService);
    BLE::Characteristic positionCharacteristic(uuidImuPositionCharateristic, NIMBLE_PROPERTY::READ);
    positionCharacteristic.SetCallback(new Ble::ImuPositionCallback);
    positionService.AddCharacteristic(&positionCharacteristic);

    BLE::Characteristic calibrationCharacteristic(uuidImuCalibrationCharateristic, NIMBLE_PROPERTY::WRITE |
                                                                                    NIMBLE_PROPERTY::READ);  
    calibrationCharacteristic.SetCallback(new Ble::ImuCalibrationCallback);
    positionService.AddCharacteristic(&calibrationCharacteristic);

    AddService(positionService);
    // ----------------------------------------------------------

    // Sleep service
    BLE::Service sleepService(uuidSleep);
    BLE::Characteristic sleepCharacteristic(uuidSleep, NIMBLE_PROPERTY::WRITE);
    sleepCharacteristic.SetCallback(new Ble::SleepCallback);
    sleepService.AddCharacteristic(&sleepCharacteristic);
    AddService(sleepService);
    // ----------------------------------------------------------

    // Firmware revision service/characteristic
    BLE::Service firmwareRevisionService(uuidFirmwareRevision);
    //TODO fetch from separate file (maintain version control)
    BLE::Characteristic firmwareRevisionCharacteristic(uuidFirmwareRevision, NIMBLE_PROPERTY::READ, "0.1.0");
    firmwareRevisionService.AddCharacteristic(&firmwareRevisionCharacteristic);
    AddService(firmwareRevisionService);
    // ----------------------------------------------------------

    // Manufacturer name service/charateristic
    BLE::Service manufacturerNameService(uuidManufacturerName);
    BLE::Characteristic manufacturerNameCharacteristic(uuidManufacturerName, NIMBLE_PROPERTY::READ, "any-sliv_labs");
    manufacturerNameService.AddCharacteristic(&manufacturerNameCharacteristic);
    AddService(manufacturerNameService);
    // ----------------------------------------------------------

    // Battery service
    BLE::Service batteryService(uuidBattery);
    BLE::Characteristic batteryCharateristic(uuidBattery, NIMBLE_PROPERTY::READ |
                                                                        NIMBLE_PROPERTY::NOTIFY);
    batteryCharateristic.SetCallback(new Ble::BatteryCallback);
    batteryService.AddCharacteristic(&batteryCharateristic);
    AddService(batteryService);
    // ----------------------------------------------------------

    // Current time service 
    BLE::Service currentTimeService(uuidCurrentTime);
    BLE::Characteristic currentTimeCharacteristic(uuidCurrentTime, NIMBLE_PROPERTY::WRITE |
                                                                                NIMBLE_PROPERTY::READ);
    currentTimeCharacteristic.SetCallback(new Ble::TimeCallback);
    currentTimeService.AddCharacteristic(&currentTimeCharacteristic);
    AddService(currentTimeService);
    // ----------------------------------------------------------

    // Device firmware update service
    BLE::Service deviceFirmwareUpdateService(uuidDeviceFirmwareUpdateService);
    BLE::Characteristic deviceFirmwareControlCharacteristic(uuidDeviceFirmwareControlCharacteristic, NIMBLE_PROPERTY::WRITE |
                                                                                                    NIMBLE_PROPERTY::READ);
    deviceFirmwareControlCharacteristic.SetCallback(new Ble::OtaControlCallback);
    deviceFirmwareUpdateService.AddCharacteristic(&deviceFirmwareControlCharacteristic);

    BLE::Characteristic deviceFirmwareDataCharacteristic(uuidDeviceFirmwareDataCharacteristic, NIMBLE_PROPERTY::WRITE);
    deviceFirmwareDataCharacteristic.SetCallback(new Ble::OtaDataCallback);
    deviceFirmwareUpdateService.AddCharacteristic(&deviceFirmwareDataCharacteristic);
    AddService(deviceFirmwareUpdateService);
    // ----------------------------------------------------------

    Advertise();
}

void Ble::Advertise() {
    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(advServiceUuid);
    adv->setScanResponse(false);
    adv->setMinPreferred(0x06);
    // adv->setMinPreferred(0x12); 
    BLEDevice::startAdvertising();
    BLE::Ble::state = Ble::ConnectionState::ADVERTISING;
}

void Ble::AddService(Service service) {
    if(server == NULL) {
        // No initialization required. Server guaranteed to be instantized.
        server = BLEDevice::createServer();
        server->setCallbacks(new ServerCallbacks);
    }

    service.self = server->createService((BLEUUID(service.uuid)));

    for(auto && characteristic : service.Characteristics) {
        // Create instances of all characteristics included in service
        characteristic->self = service.self->createCharacteristic(BLEUUID(characteristic->uuid), characteristic->property);
        characteristic->SetValue(characteristic->initValue);
        if(characteristic->callback != nullptr) characteristic->self->setCallbacks(characteristic->callback);
    }
    service.self->start();
}

void Ble::ServerCallbacks::onConnect(BLEServer * server, NimBLEConnInfo& connInfo) {
    ESP_LOGI(__FILE__, "%s:%d. BLE connection established!", __func__ ,__LINE__);
    Ble::state = Ble::ConnectionState::CONNECTED;
    BLEDevice::stopAdvertising();
}

void Ble::ServerCallbacks::onDisconnect(BLEServer * server, NimBLEConnInfo& connInfo, int reason) {
    ESP_LOGI(__FILE__, "%s:%d. BLE connection lost. Reason: %d", __func__ ,__LINE__, reason);
    Ble::state = Ble::ConnectionState::DISCONNECTED;
    BLEDevice::startAdvertising();
}

void Ble::ImuPositionCallback::onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    IMU::PositionQueueType item;
    xQueueSend(ImuPositionGetQueue, &item, 0);
    // Mind that these are two separate queues
    if(xQueueReceive(ImuPositionQueue, &item, 0)) {
        std::string text = std::to_string(item.startTime) + "," + std::to_string(item.face);
        pCharacteristic->setValue(text);
    }
    else {
        pCharacteristic->setValue(0);
    }
}

void Ble::ImuCalibrationCallback::onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    ESP_LOGI(__FILE__, "%s:%d. Calibration callback onRead", __func__ ,__LINE__);
    char item[5] = {};
    if(xQueueReceive(ImuCalibrationStateQueue, &item, 0)) {
        const std::string data(item);
        // Set calibration result. Details in IMU namespace
        pCharacteristic->setValue(item);
        // Client must clear value
    }
}

void Ble::ImuCalibrationCallback::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    ESP_LOGI(__FILE__, "%s:%d. Calibration callback onWrite", __func__ ,__LINE__);
    auto val = *pCharacteristic->getValue().data();
    //TODO do calibration cancel!
    if(val != 0) {
        // Initiate calibration
        xQueueSend(ImuCalibrationInitQueue, &val, 0);
        // Pause sleep (with timeout). Resume it using BLE sleep characteristic
        xQueueSend(SleepPauseQueue, "imuCalibration", 0);
    }
    // Clear request
    pCharacteristic->setValue(0);
}

void Ble::SleepCallback::onWrite(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo) {
    // Sleep enter request from client
    //TODO change to notify!
    auto item = 1;
    xQueueSend(SleepStartQueue, &item, 0);
}

void Ble::BatteryCallback::onRead(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo) {
    // Latest battery value is set after reading from char. Read twice to get latest data.
    ESP_LOGI(__FILE__, "%s:%d. Battery read", __func__ ,__LINE__);
    int batteryValue = 0.0;
    // Receive battery percent from battery task
    if(xQueueReceive(BatteryQueue, &batteryValue, 0)) {
        pCharacteristic->setValue(batteryValue);
    }
}

void Ble::TimeCallback::onRead(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo) {
    timeval tv = {0,0};
    gettimeofday(&tv, NULL);
    pCharacteristic->setValue(tv.tv_sec);
}

void Ble::TimeCallback::onWrite(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo) {
    // sizeof(time_t) bytes of epoch seconds time should be received
    NimBLEAttValue value = pCharacteristic->getValue();
    if(value.length() != sizeof(time_t)) {
        ESP_LOGE(__FILE__, "%s:%d. Time characteristic wrong format", __func__ ,__LINE__);
        return;
    }

    time_t receivedTime;
    memcpy(&receivedTime, value.data(), sizeof(time_t));

    timeval tv = {.tv_sec = receivedTime, .tv_usec = 0};
    settimeofday(&tv, NULL);
    ESP_LOGI(__FILE__, "%s:%d. Time updated (epoch): %d", __func__ ,__LINE__, (unsigned int) receivedTime);
}

void Ble::OtaControlCallback::onWrite(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo) {
    NimBLEAttValue chrRcv = pCharacteristic->getValue();
    unsigned int rcv = (unsigned int) *chrRcv.data();
    ESP_LOGI(__FILE__, "%s:%d. OTA Request: %d", __func__ ,__LINE__, rcv);
    if(rcv == OTA_CONTROL_REQUEST) {
        ESP_LOGI(__FILE__, "%s:%d. OTA Requested via BLE", __func__ ,__LINE__);
        partitionHandle = esp_ota_get_next_update_partition(NULL);
        if(partitionHandle == NULL) {
            ESP_LOGE(__FILE__, "%s:%d. OTA Could not get partition", __func__ ,__LINE__);
            pCharacteristic->setValue(OTA_CONTROL_REQUEST_NAK);
            return;
        }

        auto status = esp_ota_begin(partitionHandle, OTA_WITH_SEQUENTIAL_WRITES, &Ble::otaHandle);
        if(status != ESP_OK) {
            ESP_LOGE(__FILE__, "%s:%d. OTA Error. %s", __func__ ,__LINE__, esp_err_to_name(status));
            esp_ota_abort(Ble::otaHandle);
            pCharacteristic->setValue(OTA_CONTROL_REQUEST_NAK);
            return;
        }

        auto mtu = connInfo.getMTU();
        if(mtu < 250) {
            ESP_LOGW(__FILE__, "%s:%d. OTA: BLE MTU low %d", __func__ ,__LINE__, mtu);
            //TODO negotiate higher mtu
        }

        ESP_LOGI(__FILE__, "%s:%d. OTA Begin", __func__ ,__LINE__);
        xQueueSend(SleepPauseQueue, "otaUpdate", 0);

        pCharacteristic->setValue(OTA_CONTROL_REQUEST_ACK);
    } 
    else if (rcv == OTA_CONTROL_DONE) {
        ESP_LOGI(__FILE__, "%s:%d. OTA Request to end update", __func__ ,__LINE__);
        auto status = esp_ota_end(Ble::otaHandle);
        if(status != ESP_OK) {
            ESP_LOGE(__FILE__, "%s:%d. OTA Error. %s", __func__ ,__LINE__, esp_err_to_name(status));
            pCharacteristic->setValue(OTA_CONTROL_DONE_NAK);
            return;
        }

        //TODO check image version

        status = esp_ota_set_boot_partition(partitionHandle);
        if(status != ESP_OK) {
            ESP_LOGE(__FILE__, "%s:%d. OTA Error. %s", __func__ ,__LINE__, esp_err_to_name(status));
            pCharacteristic->setValue(OTA_CONTROL_DONE_NAK);
            return;
        }

        ESP_LOGI(__FILE__, "%s:%d. OTA Success. Rebooting...", __func__ ,__LINE__);
        //TODO enabled: skip image validation when exiting deep sleep
        pCharacteristic->setValue(OTA_CONTROL_DONE_ACK);
        TaskDelay(1s);
        // when calling esp_restart OS is stuck
        // workaround is to sleep after OTA, first reboot fails, then next one is fine
        auto item = 0;
        xQueueSend(SleepStartQueue, &item, 0);
    }
    else {
        ESP_LOGW(__FILE__, "%s:%d. OTA Unkown request", __func__ ,__LINE__);
    }
}

void Ble::OtaDataCallback::onWrite(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo) {
    NimBLEAttValue pkg = pCharacteristic->getValue();
    auto len = pkg.length();

    //TODO sometimes (dont know why sometimes) OTA tranmission is very slow
    ESP_LOGI(__FILE__, "%s:%d. Received packet %d", __func__ ,__LINE__, rcvPkg);
    auto status = esp_ota_write(Ble::otaHandle, pkg.c_str(), len);
    if(status == ESP_ERR_INVALID_ARG) {
        ESP_LOGE(__FILE__, "%s:%d. OTA Data write. %s", __func__ ,__LINE__, esp_err_to_name(status));
        // Notify in case of an error
        pCharacteristic->notify();
    }
    rcvPkg++;
}