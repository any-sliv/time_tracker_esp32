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
#include "sleepPause.hpp"
#include <algorithm>
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

NimBLEServer * Ble::server = NULL;
esp_ota_handle_t Ble::otaHandle = NULL;
volatile Ble::ConnectionState Ble::state = Ble::ConnectionState::IDLE;
volatile bool Ble::deliveredThisConnection = false;

const std::string advServiceUuid = "8227dcb2-30e3-11ed-a261-0242ac120002";
// <const> static constexpr char; <const> = adding it removes -Wwrite-strings warning (dunno why)
const static constexpr char * uuidImuPositionService = "7bef916a-3141-11ed-a261-0242ac120000";
const static constexpr char * uuidImuPositionCharateristic = "7bef916a-3141-11ed-a261-0242ac120001";
const static constexpr char * uuidImuCalibrationCharateristic = "7bef916a-3141-11ed-a261-0242ac120002";

const static constexpr char * uuidSleep = "646b8837-cea9-4006-be25-00c990029e90";

// Connection interval, in 1.25ms units. The default negotiated interval is ~30ms,
// which sets the floor on every ATT round trip (reads, service discovery, OTA chunks).
// Values chosen to satisfy Apple's Accessory Design Guidelines, which macOS/iOS
// enforce by rejecting non-conforming requests outright:
//   min >= 15ms and a multiple of 15ms; max >= min + 15ms; latency <= 30;
//   timeout <= 6s; max * (latency + 1) * 3 < timeout.
const static constexpr uint16_t connIntervalMin = 12;   // 15ms
const static constexpr uint16_t connIntervalMax = 24;   // 30ms
const static constexpr uint16_t connLatency     = 0;
const static constexpr uint16_t connTimeout     = 200;  // 2000ms, in 10ms units

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
    // NimBLE 2.x: init() reports success instead of returning void. The name given here
    // is the GAP Device Name characteristic - it is no longer put in the advertisement
    // (see Advertise()).
    if(!NimBLEDevice::init("Time tracker")) {
        ESP_LOGE(__FILE__, "%s:%d. NimBLE init failed", __func__ ,__LINE__);
        return;
    }

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

    // NimBLE 2.x: services are registered when the server starts, not by
    // NimBLEService::start() (now a deprecated no-op). startAdvertising() would do
    // this implicitly; calling it here keeps the failure visible in the log.
    if(server != nullptr && !server->start()) {
        ESP_LOGE(__FILE__, "%s:%d. GATT server failed to start", __func__ ,__LINE__);
    }

    Advertise();
}

void Ble::Advertise() {
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(advServiceUuid);
    // NimBLE 2.x no longer enables the scan response by default; keep it off. A scan
    // response costs an extra SCAN_REQ/SCAN_RSP exchange per scanning central, and
    // nothing in the client contract needs it (docs/bleApi.md keys on the UUID below).
    adv->enableScanResponse(false);
    // Advertised Slave Connection Interval Range. Was 0x06 (7.5ms) with no max, which
    // is below Apple's 15ms floor and so gets ignored by macOS/iOS.
    adv->setPreferredParams(connIntervalMin, connIntervalMax);
    // No setName(): NimBLE 2.x stopped advertising the name implicitly, and there is no
    // room for it anyway - flags (3) + the 128-bit service UUID (18) + the interval
    // range (6) already fill 27 of the 31 legacy payload bytes. Clients still read the
    // full "Time tracker" from the GAP Device Name characteristic after connecting.
    NimBLEDevice::startAdvertising();
    BLE::Ble::state = Ble::ConnectionState::ADVERTISING;
}

void Ble::AddService(Service service) {
    if(server == NULL) {
        // No initialization required. Server guaranteed to be instantized.
        server = NimBLEDevice::createServer();
        server->setCallbacks(new ServerCallbacks);
    }

    service.self = server->createService((NimBLEUUID(service.uuid)));

    for(auto && characteristic : service.Characteristics) {
        // Create instances of all characteristics included in service
        characteristic->self = service.self->createCharacteristic(NimBLEUUID(characteristic->uuid), characteristic->property);
        characteristic->SetValue(characteristic->initValue);
        if(characteristic->callback != nullptr) characteristic->self->setCallbacks(characteristic->callback);
    }
    // No service.self->start() here: in NimBLE 2.x all services are registered in one
    // go by NimBLEServer::start(), called at the end of Init().
}

void Ble::ServerCallbacks::onConnect(NimBLEServer * server, NimBLEConnInfo& connInfo) {
    ESP_LOGI(__FILE__, "%s:%d. BLE connection established!", __func__ ,__LINE__);
    Ble::state = Ble::ConnectionState::CONNECTED;
    Ble::deliveredThisConnection = false;
    NimBLEDevice::stopAdvertising();

    // Ask for a faster connection interval. Every ATT round trip costs at least one
    // interval, so this shortens reads, service discovery and OTA alike. The central
    // may refuse; onConnParamsUpdate/the NimBLE log will show what was actually agreed.
    server->updateConnParams(connInfo.getConnHandle(), connIntervalMin, connIntervalMax,
                             connLatency, connTimeout);
}

void Ble::ServerCallbacks::onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) {
    // Position reads batch into MTU-3 bytes, so this is the effective batch size
    ESP_LOGI(__FILE__, "%s:%d. MTU negotiated: %u (batch budget %u B)",
             __func__ ,__LINE__, MTU, MTU > 3 ? MTU - 3 : 0);
}

void Ble::ServerCallbacks::onDisconnect(NimBLEServer * server, NimBLEConnInfo& connInfo, int reason) {
    ESP_LOGI(__FILE__, "%s:%d. BLE connection lost. Reason: %d", __func__ ,__LINE__, reason);
    Ble::state = Ble::ConnectionState::DISCONNECTED;
    // NimBLE 2.x dropped the implicit restart on disconnect (advertiseOnDisconnect() now
    // defaults off), so this call is the only thing putting the device back on air.
    NimBLEDevice::startAdvertising();
}

void Ble::ImuPositionCallback::onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    // Budget for one batch. MTU-3 is the ATT payload; never assume it, the client
    // negotiates it. Clamped so the reply always fits the queue item.
    const uint16_t mtu = connInfo.getMTU();
    const uint16_t budget = std::min<uint16_t>(mtu > 3 ? mtu - 3 : 1,
                                               IMU::PositionBatchMaxLen - 1);

    if(xQueueSend(ImuPositionGetQueue, &budget, 0) != pdTRUE) {
        // Depth-1 queue: a second read arriving before ImuTask serviced the first
        // silently drops the request, and this read returns stale/empty.
        ESP_LOGW(__FILE__, "%s:%d. Position request dropped, read again", __func__ ,__LINE__);
    }

    // Mind that these are two separate queues. The value set here is the response to
    // the PREVIOUS read, which is why clients are told to read twice.
    char batch[IMU::PositionBatchMaxLen] = {0};
    if(xQueueReceive(ImuPositionQueue, batch, 0)) {
        batch[IMU::PositionBatchMaxLen - 1] = '\0';
        pCharacteristic->setValue(std::string(batch));
        Ble::deliveredThisConnection = true;
    }
    else {
        // Keep this byte-identical: setValue(0) deduces int, i.e. four zero bytes.
        // That is what the client's drain loop terminates on - do not "tidy" it into
        // an empty string.
        pCharacteristic->setValue(0);

        // Backlog drained. Only sleep early if this connection actually delivered
        // something, otherwise a client connecting purely to calibrate or set the
        // clock would be hung up by its very first empty read.
        if(Ble::deliveredThisConnection) {
            ESP_LOGI(__FILE__, "%s:%d. Backlog drained, sleeping", __func__ ,__LINE__);
            auto mode = SleepStartMode::Graceful;
            xQueueSend(SleepStartQueue, &mode, 0);
        }
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
        auto reason = SleepPauseReason::ImuCalibration;
        xQueueSend(SleepPauseQueue, &reason, 0);
    }
    // Clear request
    pCharacteristic->setValue(0);
}

void Ble::SleepCallback::onWrite(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo) {
    // Sleep enter request from client
    //TODO change to notify!
    auto mode = SleepStartMode::Immediate;
    xQueueSend(SleepStartQueue, &mode, 0);
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

    // The client just proved it is active. Sleep deadlines are monotonic now so this
    // jump no longer strands them in the past, but hold awake anyway.
    auto reason = SleepPauseReason::TimeSet;
    xQueueSend(SleepPauseQueue, &reason, 0);
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
        auto reason = SleepPauseReason::OtaUpdate;
        xQueueSend(SleepPauseQueue, &reason, 0);

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
        // Graceful, so the DONE_ACK this callback just wrote reaches the client
        auto mode = SleepStartMode::Graceful;
        xQueueSend(SleepStartQueue, &mode, 0);
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