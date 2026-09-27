# Time tracker ESP32 BLE API

This documentation provides information on the Bluetooth Low Energy (BLE) communication protocol API used in a time tracker. The documentation is organized into sections that correspond to the different services supported by the application. Each service includes one or more characteristics that describe the data being transmitted over BLE. The documentation also includes guidelines for developers and users.
</br>


### Tracker goes zzz...
The device is sleeping most of the time, what also means its BLE controller is turned off. Waking up happens once per five minutes or position has changed, and it advertises for a few seconds — so be quick and listen for it... basically always.
</br>
Once you are **connected**, the tracker holds itself awake for up to **30 seconds** rather than hanging up mid-transfer. It ends the session early when either the position backlog has been drained (see Position below) or you write the Sleep characteristic. Note that after it sleeps it may not advertise again for up to five minutes, so finish everything you need in one session.
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
    | int64_t | 8 | Epoch (unix) time | READ + WRITE |

    Device has no RTC battery, thus it might lose time. Using this characteristic device can have its system time updated.
    Device updates value onRead action. Read twice to get actual value.

- **Sleep** (UUID: --)
  - **Sleep** (UUID: 646b8837-cea9-4006-be25-00c990029e90)
    | Data | Length (bytes) | Description | Properties |
    | -------- | -------- | -------- | -------- | 
    | uint8_t | 1 | Sleep request | WRITE |

    Device stays in sleep mode most of the time. Use this characteristic to force device to enter sleep mode. Recommended to use after done with processing all data from the tracker. Write (uint8_t)<1> to force sleep.

- **IMU** (UUID: 7bef916a-3141-11ed-a261-0242ac120000)
  - **Position** (UUID: 7bef916a-3141-11ed-a261-0242ac120001)
    | Data | Length (bytes) | Description | Properties |
    | -------- | -------- | -------- | -------- | 
    | \<startTime>,\<face>[;\<startTime>,\<face>]... | variable, up to MTU-3 | Batch of tracker positions and their start times | READ |

    ASCII text. One read returns **as many records as fit in the negotiated MTU minus 3 bytes**, separated by `;`. Records are **oldest first**, and ordering holds across successive reads, so a client can simply append each batch.

    Example data (three records in one read):
    ```
    1688587818,8;1688589019,4;1688590151,8
    ```

    - `startTime` is decimal epoch (unix) seconds at which the face was registered.
    - `face` is the cube face. It may read **4294967295** (`-1` as unsigned), which means the tracker saw a position that matches none of its calibrated faces — treat it as "unknown", not as a real face.
    - **A single record is a valid batch of one**, so a client written against the older one-record-per-read format keeps working unchanged.
    - At a very small (unnegotiated, 23-byte) MTU a single record can exceed the budget. The tracker sends it anyway rather than stalling; the value is retrievable via a read-blob continuation.

    Each tracker flip is saved in order to be sent later, so a backlog accumulates while there is no BLE connection.
    </br> Device updates value onRead action. Read twice to get actual value. First read might be zero, always read at least twice.
    BLE Client should read as long as value in characteristic is other than 0, due to fact there might be more data to read than just from one position change.
    </br> Once the tracker has handed over at least one record and a subsequent read returns 0, it treats the backlog as drained and goes to sleep shortly after — so issue any other work (calibration, time set) before finishing the drain, or keep the connection busy.

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

