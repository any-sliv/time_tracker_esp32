#pragma once

#include <list>
#include "dateTime.hpp"
#include "NimBLEServer.h"
#include "NimBLEDevice.h"

namespace BLE {

void BleTask(void *pvParameters);
/**
 * @brief IMU device task/thread. Execution managed by OS.
 */

class Characteristic {
public:
    BLECharacteristic *self = nullptr;
    BLECharacteristicCallbacks *callback = nullptr;
    std::string uuid;
    uint32_t property;
    uint16_t initValue;
    Characteristic(const std::string& _uuid, uint32_t _property, const uint16_t _initValue = 0) : 
                    uuid(_uuid), property(_property), initValue(_initValue) {};
    ~Characteristic() {};

    void SetValue(std::string &value) {
        self->setValue(value);
    }

    void SetValue(uint16_t value) {
        self->setValue(value);
    }

    const uint8_t * GetValue() {
        return self->getValue().data();
    }

    void SetCallback(BLECharacteristicCallbacks * cb) {
        callback = cb;
    }

    void Notify() {
        self->notify();
    }
};

class Service {
public:
    Service() = delete;
    Service(const std::string& _uuid) : uuid(_uuid) {};

    BLEService *self = nullptr;
    std::list<Characteristic *> Characteristics;
    std::string uuid;

    void AddCharacteristic(Characteristic * bleChar) {
        Characteristics.push_back(bleChar);
    }
};

class Ble {
    static BLEServer * server;
    
public:
    // Callbacks of BLE server (GAP)
    class ServerCallbacks : public NimBLEServerCallbacks {
        void onConnect(BLEServer * server, NimBLEConnInfo& connInfo);
        void onDisconnect(BLEServer * server, NimBLEConnInfo& connInfo, int reason);
    };

    // Callbacks of IMU characteristics in BLE communication
    class ImuPositionCallback : public NimBLECharacteristicCallbacks {
        // Used to send data about time spend in <face> position
        // At read it will put new data (if available) into characteristic
        void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo);
    };

    // Callbacks of IMU characteristics in BLE communication
    class ImuCalibrationCallback : public NimBLECharacteristicCallbacks {
        // Used in calibration process
        void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo);
        // Used to initiate calibration
        void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo);
    };

    // Callbacks of sleep characteristics in BLE communication
    class SleepCallback : public NimBLECharacteristicCallbacks {
        void onWrite(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo);
    };

    // Callbacks of battery characteristics in BLE communication
    class BatteryCallback : public NimBLECharacteristicCallbacks {
        void onRead(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo);
    };

    // Callbacks of time characteristics in BLE communication
    class TimeCallback : public NimBLECharacteristicCallbacks {
        void onRead(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo);
        void onWrite(NimBLECharacteristic * pCharacteristic, NimBLEConnInfo& connInfo);
    };

    static void Init();

    static void Advertise();

    enum class ConnectionState { IDLE, ADVERTISING, CONNECTED, DISCONNECTED };
    static ConnectionState state;

    static void AddService(Service service);
};

} // namespace BLE end --------------------
