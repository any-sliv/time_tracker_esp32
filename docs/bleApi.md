# Time tracker ESP32 BLE API

This documentation provides information on the Bluetooth Low Energy (BLE) communication protocol API used in a time tracker. The documentation is organized into sections that correspond to the different services supported by the application. Each service includes one or more characteristics that describe the data being transmitted over BLE. The documentation also includes guidelines for developers and users.
</br>


### Tracker goes zzz...
The device is sleeping most of the time, what also means its BLE controller is turned off. Waking up happens once per five minutes or position has changed. Bluetooth wakes up for <5s, so be quick and listen for it... basically always.
</br>
Mind that tracker is a low power device and its juice comes from battery, so try use/read/write as least and quick as possible.
</br>

## Services and characteristics:
- **Service**
  - **Characteristic**
  - ...

### **Time tracker advertise** UUID: 8227dcb2-30e3-11ed-a261-0242ac120002
</br>

- **Device manufacturer name** (UUID:--)
  - **Device manufacturer name** (UUID:2A29)
    </br>
    .
    
- **Firmware revision** (UUID:--)
  - **Firmware revision** (UUID:2A26)
    </br>
     .
- **Battery** (UUID: --)
  - **Battery**
    </br>
    This is BT SIG adopted service. Data is a battery level [%]. Details: https://www.bluetooth.com/specifications/specs/battery-service/
    </br>
    Device updates value onRead action. Read twice to get actual value.

- **Current Time** (UUID: --)
  - **Current Time** (UUID: 2A2B)
    </br>
    | Data | Length (bytes) | Description | Properties |
    | -------- | -------- | -------- | -------- | 
    | uint32_t | 8 | Epoch (unix) time | READ + WRITE |

    Device has no RTC battery, thus it might lose time. Using this characteristic device can have its system time updated.
    Device updates value onRead action. Read twice to get actual value.

- **Sleep** (UUID: --)
  - **Sleep** (UUID: 646b8837-cea9-4006-be25-00c990029e91)
    | Data | Length (bytes) | Description | Properties |
    | -------- | -------- | -------- | -------- | 
    | uint8_t | 1 | Sleep request | WRITE |

    Device stays in sleep mode most of the time. Use this characteristic to force device to enter sleep mode. Recommended to use after done with processing all data from the tracker. Write (uint8_t)<1> to force sleep.

- **IMU** (UUID: 7bef916a-3141-11ed-a261-0242ac120000)
  - **Position** (UUID: 7bef916a-3141-11ed-a261-0242ac120001)
    | Data | Length (bytes) | Description | Properties |
    | -------- | -------- | -------- | -------- | 
    | \<startTime>,\<cubeFace> | 10 | Tracker last position and start time | READ |

    Example data: 
    | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
    | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | 
    | 0x00 | 0x00 | 0x00 | 0x00 | 0x63 | 0xF0 | 0xD2 | 0x9E | 0x2C | 0x02 |
    | Time b.7 | Time b.6 | Time b.5 | Time b.4 | Time b.3 | Time b.2 | Time b.1 | Time b.0 | Comma "," | Cube's face |

    Each tracker flip, this data is set in characteristic **OR** saved in order to send later when has no BLE connection. Start time is the time at which new position/face was registered.
    </br> Device updates value onRead action. Read twice to get actual value. First read might be zero, always read at least twice.
    BLE Client should read as long as value in characteristic is other than 0, due to fact there might be more data to read than just from one position change.

  - **Calibration** (UUID: 7bef916a-3141-11ed-a261-0242ac120002)
    </br>
    | Data | Length (bytes) | Description | Properties |
    | -------- | -------- | -------- | -------- | 
    | \<status>,\<face> | 4-5 | Client request or server response | WRITE + READ |

    Calibration characteristic is used during initial configuration to calibrate IMU/accelometer. Requests from client initiate calibration and server responds with result and calibrated face number. Client initiates calibration of each position/face/wall with writing (uint8_t)<1> to the characteristic. Table below represents values server might set as a result.

    | Value | Definition |
    | -------- | -------- | 
    | -1 | Error, calibration failed |
    | 0 | Idle, calibration not initiated |
    | 1 | Position already exists/duplicate | 
    | 2 | Position registered OK | 
    | 3 | Calibration done |

    Flow of calibration process:
    - Client requests calibration - write (uint8_t)<1> to characteristic
    - Tracker responds with a write to characteristic with response from table above
    - Client reads from characteristic. If position OK/already exists - flip the cube/tracker
    - Client requests calibration - write (uint8_t)<1> to characteristic
    - ...
    - Until done

uuidDeviceFirmwareUpdateService = "00009921-1212-efde-1523-785feabcd123"; 
uuidDeviceFirmwareDataCharacteristic = "00009921-1212-efde-1523-785feabcd124"; 
uuidDeviceFirmwareControlCharacteristic = "00009921-1212-efde-1523-785feabcd125"; 

- **Device firmware update** (UUID:00009921-1212-efde-1523-785feabcd123)
  - **Device firmware control** (UUID:00009921-1212-efde-1523-785feabcd125)
    </br>
    DFU characteristic, use it for initiating update and checking status.

    | Value | Definition | Description |
    | -------- | -------- | -------- |
    | 0 | OTA Start Request | Send to server to request DFU begin |
    | 1 | OTA Start Request acknowledged | Server sets value |
    | 2 | OTA Start Request not acknowledged | Server sets value |
    | 3 | OTA Done Request | Send to server to request DFU end |
    | 4 | OTA Done Request acknowledged | Server sets value | 
    | 5 | OTA Done Request not acknowledged | Server sets value |

    Process flow:
    </br>
    Client requests DFU and after second reads from characteristic what server has responded. In case of success split firmware file (.bin) into chunks of a `chunkSize = BLE MTU size - 3`. Send all the chunks to data characteristic (no response). After sending all data, write a done request to this characteristic, wait 3-5 seconds and read a value if end has been acknowledged. If so, device will reboot in a couple of seconds.

  - **Device firmware update data** (UUID:00009921-1212-efde-1523-785feabcd124)
    </br>
    Send here chunks of firmware file

