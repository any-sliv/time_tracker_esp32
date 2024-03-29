/**
 * @file mpu6050.cpp
 * @author Maciej Sliwinski
 * @brief This file is a part of time_tracker_esp32 project.
 *
 * The code is distributed under the MIT License.
 * See the LICENCE file for more details.
 */

#include "mpu6050.hpp"
#include "kalmanfilter.hpp"
#include <cmath>

#define ORIGINAL_OUTPUT			 (0)
#define ACC_FULLSCALE        	 (2)
#define GYRO_FULLSCALE			 (250)

#if ORIGINAL_OUTPUT == 0
	#if  ACC_FULLSCALE  == 2
		#define AccAxis_Sensitive (float)(16384)
	#elif ACC_FULLSCALE == 4
		#define AccAxis_Sensitive (float)(8192)
	#elif ACC_FULLSCALE == 8
		#define AccAxis_Sensitive (float)(4096)
	#elif ACC_FULLSCALE == 16
		#define AccAxis_Sensitive (float)(2048)
	#endif 
		
	#if   GYRO_FULLSCALE == 250
		#define GyroAxis_Sensitive (float)(131.0)
	#elif GYRO_FULLSCALE == 500
		#define GyroAxis_Sensitive (float)(65.5)
	#elif GYRO_FULLSCALE == 1000
		#define GyroAxis_Sensitive (float)(32.8)
	#elif GYRO_FULLSCALE == 2000
		#define GyroAxis_Sensitive (float)(16.4)
	#endif
		
#else
	#define AccAxis_Sensitive  (1)
	#define GyroAxis_Sensitive (1)
#endif

MPU6050::MPU6050(gpio_num_t scl, gpio_num_t sda, i2c_port_t port) {
    i2c = new I2C(scl, sda, port);
}

MPU6050::~MPU6050() {
    delete(i2c);
}

bool MPU6050::init() {
    if (!i2c -> slave_write(MPU6050_ADDR, PWR_MGMT_1  , 0x00))
        return false;
    if (!i2c -> slave_write(MPU6050_ADDR, SMPLRT_DIV  , 0x07))
        return false;
    if (!i2c -> slave_write(MPU6050_ADDR, CONFIG      , 0x07))
        return false;
    if (!i2c -> slave_write(MPU6050_ADDR, GYRO_CONFIG , 0x18))
        return false;
    if (!i2c -> slave_write(MPU6050_ADDR, ACCEL_CONFIG, 0x01))
        return false;
    if (!i2c -> slave_write(MPU6050_ADDR, PWR_MGMT_1, 0x00))
        return false;
    enableSleepCycling();
    
    return true;
}

float MPU6050::getAccX() {
    uint8_t r[2];
    i2c -> slave_read(MPU6050_ADDR, ACCEL_XOUT_H, r, 2);
    short accx = r[0] << 8 | r[1];
    return (float)accx / AccAxis_Sensitive;
}

float MPU6050::getAccY() {
    uint8_t r[2];
    i2c -> slave_read(MPU6050_ADDR, ACCEL_YOUT_H, r, 2);
    short accy = r[0] << 8 | r[1];
    return (float)accy / AccAxis_Sensitive;
}

float MPU6050::getAccZ() {
    uint8_t r[2];
    i2c -> slave_read(MPU6050_ADDR, ACCEL_ZOUT_H, r, 2);
    short accz = r[0] << 8 | r[1];
    return (float)accz / AccAxis_Sensitive;
}

float MPU6050::getGyroX() {
    uint8_t r[2];
    i2c -> slave_read(MPU6050_ADDR, GYRO_XOUT_H, r, 2);
    short gyrox = r[0] << 8 | r[1];
    return (float)gyrox / GyroAxis_Sensitive;
}

float MPU6050::getGyroY() {
    uint8_t r[2];
    i2c -> slave_read(MPU6050_ADDR, GYRO_YOUT_H, r, 2);
    short gyroy = r[0] << 8 | r[1];
    return (float)gyroy / GyroAxis_Sensitive;
}

float MPU6050::getGyroZ() {
    uint8_t r[2];
    i2c -> slave_read(MPU6050_ADDR, GYRO_ZOUT_H, r, 2);
    short gyroz = r[0] << 8 | r[1];
    return (float)gyroz / GyroAxis_Sensitive;
}

void MPU6050::enableSleepCycling() {
    uint8_t r1, r2;
    i2c->slave_read(MPU6050_ADDR, PWR_MGMT_1, &r1, 1);
    i2c->slave_read(MPU6050_ADDR, PWR_MGMT_2, &r2, 1);

    /*
        SLEEP When set to 1, this bit puts the MPU-60X0 into sleep mode.
        CYCLE When this bit is set to 1 and SLEEP is disabled, the MPU-60X0 will cycle between sleep mode and 
        waking up to take a single sample of data from active sensors at a rate determined by LP_WAKE_CTRL (register 108).
    */

    r1 = r1 || 0b00101000; // Set cycle and temp_disable bit
    i2c->slave_write(MPU6050_ADDR, PWR_MGMT_1, r1);

    r2 = r2 || 0b01000111; // Set wakeup frequency to 5 Hz and disable gyroscope
    i2c->slave_write(MPU6050_ADDR, PWR_MGMT_2, r2);
}