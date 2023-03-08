# Time tracker ESP32 BLE API

This documentation provides information on the Bluetooth Low Energy (BLE) communication protocol API used in a time tracker. The documentation is organized into sections that correspond to the different services supported by the application. Each service includes one or more characteristics that describe the data being transmitted over BLE. The documentation also includes guidelines for developers and users.
</br>


### Tracker goes zzz...
The device is sleeping most of the time, what also means its BLE controller is turned off. Waking up happens once per five minutes or position has changed. todo: wakes up and advertises for how long?
</br>
Mind that tracker is a low power device and its juice comes from battery, so try use/read/write as least and quick as possible.
</br>

## Services and characteristics:
- **Service**
  - **Characteristic**
  - ...

**Time tracker advertise** UUID: 8227dcb2-30e3-11ed-a261-0242ac120002
</br>

- **Battery** (UUID: 180F)
  - **Battery**
    </br>
    This is BT SIG adopted service. Details: https://www.bluetooth.com/specifications/specs/battery-service/
    </br>
    Device updates value onRead action. Read twice to get actual value.

- **Current Time** (UUID: 1805)
  - **Current Time** (UUID: 2A2B)
    </br>
    | Data | Length (bytes) | Description | Properties |
    | -------- | -------- | -------- | -------- | 
    | uint32_t | 4 | Epoch (unix) time | READ + WRITE |

    Device has no RTC battery, thus it might lose time. Using this characteristic device can have its system time updated.
    Device updates value onRead action. Read twice to get actual value.

- **Sleep** (UUID: 646b8837-cea9-4006-be25-00c990029e90)
  - **Sleep** (UUID: 646b8837-cea9-4006-be25-00c990029e91)
    | Data | Length (bytes) | Description | Properties |
    | -------- | -------- | -------- | -------- | 
    | uint8_t | 1 | Sleep request | WRITE |

    Device stays in sleep mode most of the time. Use this characteristic to force device to enter sleep mode. Recommended to use after done with processing all data from the tracker. Write (uint8_t)<1> to force sleep.

- **IMU** (UUID: 7bef916a-3141-11ed-a261-0242ac120000)
  - **Position** (UUID: 7bef916a-3141-11ed-a261-0242ac120001)
    | Data | Length (bytes) | Description | Properties |
    | -------- | -------- | -------- | -------- | 
    | \<startTime>,\<cubeFace> | 6 | Tracker last position and start time | READ |

    Example data: 
    | 0 | 1 | 2 | 3 | 4 | 5 | 
    | -------- | -------- | -------- |-------- |-------- | -------- | 
    | 0x63 | 0xF0 | 0xD2 | 0x9E | 0x2C | 0x02 |
    | Time b.0 | Time b.1 | Time b.2 | Time b.3 | Comma "," | Cube's face |

    Each tracker flip, this data is set in characteristic **OR** saved in order to send later when has no BLE connection. Start time is the time at which new position/face was registered.
    </br> Device updates value onRead action. Read twice to get actual value. First read might be zero, always read at least twice.
    BLE Client should read as long as value in characteristic is other than 0, due to fact there might be more data to read than just from one position change.

  - **Calibration** (UUID: 7bef916a-3141-11ed-a261-0242ac120002)
    </br>
    | Data | Length (bytes) | Description | Properties |
    | -------- | -------- | -------- | -------- | 
    | uint8_t | 1 | Client request or server response | WRITE + READ |

    Calibration characteristic is used during initial configuration to calibrate IMU/accelometer. Requests from client initiate calibration and server responds with result. Client initiates calibration of each position/face/wall with writing (uint8_t)<1> to the characteristic. Table below represents values server might set as a result.

    | Value | Definition |
    | -------- | -------- | 
    | -1 | Error, calibration failed |
    | 0 | Idle, calibration not initiated |
    | 1 | Position already exists | 
    | 2 | Position registered OK | 
    | 3 | Calibration done |

    Flow of calibration process:
    - Client requests calibration - write (uint8_t)<1> to characteristic
    - Tracker responds with a write to characteristic with response from table above
    - Client reads from characteristic. If position OK/already exists - flip the cube/tracker
    - Client requests calibration - write (uint8_t)<1> to characteristic
    - ...
    - Until done