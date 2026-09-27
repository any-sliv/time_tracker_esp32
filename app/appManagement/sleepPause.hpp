/**
 * @file sleepPause.hpp
 * @author Maciej Sliwinski
 * @brief This file is a part of time_tracker_esp32 project.
 *
 * The code is distributed under the MIT License.
 * See the LICENCE file for more details.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Reason codes carried by SleepPauseQueue, used to defer deep sleep.
 *
 * This used to be a 16-byte string compared with std::string on the receiving
 * side. That silently failed whenever a caller passed the address of a pointer
 * variable rather than the literal itself (imu.cpp did exactly that, so the
 * per-record deferral never once fired), and the correct call sites still
 * over-read past the end of their string literals. An enum removes both.
 */
enum class SleepPauseReason : uint8_t {
    ImuCalibration,   ///< Calibration in progress, hold awake for minutes
    ImuSendPosition,  ///< A batch of records was handed to BLE, give it time to land
    OtaUpdate,        ///< Firmware update streaming, hold awake for a long while
    TimeSet,          ///< Client just set the clock, so it is demonstrably active
};

/**
 * @brief How urgently a SleepStartQueue request wants the device asleep.
 *
 * Graceful exists because sleep() calls NimBLEDevice::deinit(), which tears the stack
 * down hard. Sleeping the instant a BLE callback asks can drop the ATT response that
 * callback was still writing, so the client sees a disconnect instead of a clean end.
 */
enum class SleepStartMode : uint8_t {
    Graceful = 0,   ///< Sleep shortly, letting in-flight BLE traffic land
    Immediate = 1,  ///< Sleep on the next tick
};

/* --------------------------------------- END OF FILE ------------------------------------------- */
