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
extern QueueHandle_t ImuCalibrationInitQueue;
extern QueueHandle_t ImuCalibrationStateQueue;
extern QueueHandle_t SleepPauseQueue;
extern QueueHandle_t SleepStartQueue;

BLEServer * Ble::server = NULL;
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

        //todo pairing process???

        //todo register ESP_ERRORS and send them thru ble?

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

    BLE::Characteristic calibrationCharacteristic(uuidImuCalibrationCharateristic, NIMBLE_PROPERTY::WRITE_NR |
                                                                                    NIMBLE_PROPERTY::READ);  
    calibrationCharacteristic.SetCallback(new Ble::ImuCalibrationCallback);
    positionService.AddCharacteristic(&calibrationCharacteristic);
    AddService(positionService);
    // ----------------------------------------------------------

    // Sleep service
    BLE::Service sleepService(uuidSleep);
    BLE::Characteristic sleepCharacteristic(uuidSleep, NIMBLE_PROPERTY::WRITE_NR);
    sleepCharacteristic.SetCallback(new Ble::SleepCallback);
    sleepService.AddCharacteristic(&sleepCharacteristic);
    AddService(sleepService);
    // ----------------------------------------------------------

    // Device firmware update service
    BLE::Service deviceFirmwareUpdateService(uuidDeviceFirmwareUpdateService);
    BLE::Characteristic deviceFirmwareControlCharacteristic(uuidDeviceFirmwareControlCharacteristic, NIMBLE_PROPERTY::WRITE_NR |
                                                                                                    NIMBLE_PROPERTY::READ);
    //todo set callback
    deviceFirmwareUpdateService.AddCharacteristic(&deviceFirmwareControlCharacteristic);
    BLE::Characteristic deviceFirmwareDataCharacteristic(uuidDeviceFirmwareDataCharacteristic, NIMBLE_PROPERTY::WRITE_NR);
    //todo set callback
    deviceFirmwareUpdateService.AddCharacteristic(&deviceFirmwareDataCharacteristic);
    AddService(deviceFirmwareUpdateService);
    // ----------------------------------------------------------

    // Firmware revision service/characteristic
    BLE::Service firmwareRevisionService(uuidFirmwareRevision);
    //todo fetch from separate file (maintain version control)
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
    BLE::Characteristic currentTimeCharacteristic(uuidCurrentTime, NIMBLE_PROPERTY::WRITE_NR |
                                                                                NIMBLE_PROPERTY::READ);
    currentTimeCharacteristic.SetCallback(new Ble::TimeCallback);
    currentTimeService.AddCharacteristic(&currentTimeCharacteristic);
    AddService(currentTimeService);
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
    //todo stopping adv quickly sometimes makes device not able to connect to
    BLEDevice::stopAdvertising();
}

void Ble::ServerCallbacks::onDisconnect(BLEServer * server, NimBLEConnInfo& connInfo, int reason) {
    ESP_LOGI(__FILE__, "%s:%d. BLE connection lost. Reason: %d", __func__ ,__LINE__, reason);
    Ble::state = Ble::ConnectionState::DISCONNECTED;
    BLEDevice::startAdvertising();
}

void Ble::ImuPositionCallback::onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    IMU::PositionQueueType item;
    // Receive from IMU current face cube is lying on
    if(xQueueReceive(ImuPositionQueue, &item, 0) == pdTRUE) {
        std::string text = std::to_string(item.startTime) + "," + std::to_string(item.face);
        ESP_LOGI(__FILE__, "%s:%d. BLE got item from IMU %s", __func__ ,__LINE__, text.c_str());
        
        pCharacteristic->setValue(text);
    } else {
        pCharacteristic->setValue(0);
    }

    //todo delay deep_sleep by 50ms to let client read
};

void Ble::ImuCalibrationCallback::onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    IMU::PositionQueueType item;
    ESP_LOGI(__FILE__, "%s:%d. Calibration callback onRead", __func__ ,__LINE__);
    if(xQueueReceive(ImuCalibrationStateQueue, &item, 0) == pdTRUE) {
        std::string data = std::to_string(item.face);
        data += ",";
        data += std::to_string(item.startTime);

        // Set calibration result. Details in IMU namespace
        ESP_LOGI(__FILE__, "%s:%d. onRead value set: %s", __func__ ,__LINE__, data.c_str());
        pCharacteristic->setValue(data);
        // Client must clear value
    }
}

void Ble::ImuCalibrationCallback::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    ESP_LOGI(__FILE__, "%s:%d. Calibration callback onWrite", __func__ ,__LINE__);
    auto val = *pCharacteristic->getValue().data();
    //todo do calibration cancel!
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
    auto item = 1;
    xQueueSend(SleepStartQueue, &item, 0);
}

void Ble::BatteryCallback::onRead(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo) {
    // Latest battery value is set after reading from char. Read twice to get latest data.
    ESP_LOGI(__FILE__, "%s:%d. Battery read", __func__ ,__LINE__);
    float batteryValue = 0.0;
    // Receive battery percent from battery task
    if(xQueueReceive(BatteryQueue, &batteryValue, 0) == pdTRUE) {
        pCharacteristic->setValue(batteryValue);
    }
}

void Ble::TimeCallback::onRead(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo) {
    timeval tv = {0,0};
    gettimeofday(&tv, NULL);
    pCharacteristic->setValue(tv.tv_sec);
}

void Ble::TimeCallback::onWrite(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo) {
    // 4 bytes of epoch seconds time should be received
    NimBLEAttValue value = pCharacteristic->getValue();
    if(value.length() != sizeof(time_t)) {
        ESP_LOGE(__FILE__, "%s:%d. Time characteristic wrong format", __func__ ,__LINE__);
        return;
    }

    time_t receivedTime;
    memcpy(&receivedTime, value.data(), 4);

    timeval tv = {.tv_sec = receivedTime, .tv_usec = 0};
    settimeofday(&tv, NULL);
    ESP_LOGI(__FILE__, "%s:%d. Time updated (epoch): %d", __func__ ,__LINE__, (uint32_t) receivedTime);
}