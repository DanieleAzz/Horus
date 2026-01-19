#include "bme280.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cstring>
#include <cmath>

// BME280 Registers (From Datasheet)
#define REG_ID 0xD0
#define REG_RESET 0xE0
#define REG_CTRL_HUM 0xF2
#define REG_STATUS 0xF3
#define REG_CTRL_MEAS 0xF4
#define REG_CONFIG 0xF5
#define REG_DATA 0xF7

namespace horus {

BME280::BME280(uint8_t i2cAddress, int busId) 
    : i2c_fd(-1), busId(busId), deviceAddress(i2cAddress), t_fine(0) {}

BME280::~BME280() {
    if (i2c_fd >= 0) close(i2c_fd);
}

bool BME280::init() {
    // 1. Open I2C Bus
    std::string filename = "/dev/i2c-" + std::to_string(busId);
    if ((i2c_fd = open(filename.c_str(), O_RDWR)) < 0) {
        std::cerr << "[BME280] Failed to open I2C bus: " << filename << std::endl;
        return false;
    }

    // 2. Select Device
    if (ioctl(i2c_fd, I2C_SLAVE, deviceAddress) < 0) {
        std::cerr << "[BME280] Failed to acquire bus access." << std::endl;
        return false;
    }

    // 3. Check ID (Should be 0x60 for BME280)
    uint8_t id;
    readRegs(REG_ID, &id, 1);
    if (id != 0x60) {
        std::cerr << "[BME280] ID mismatch. Expected 0x60, got " << std::hex << (int)id << std::endl;
        return false;
    }

    // 4. Load Calibration Data
    if (!readCalibrationData()) return false;

    // 5. Configure Sensor
    // humidity oversampling x1
    writeReg(REG_CTRL_HUM, 0x01); 
    
    // temp oversampling x1, press oversampling x1, Normal Mode
    writeReg(REG_CTRL_MEAS, 0x27); 
    
    // standby 1000ms, filter off
    writeReg(REG_CONFIG, 0xA0);   

    return true;
}

BME280Data BME280::readAll() {
    BME280Data data = {0, 0, 0};
    
    // BME280 data is burst read from 0xF7 to 0xFE (8 bytes)
    // press_msb, press_lsb, press_xlsb, temp_msb, temp_lsb, temp_xlsb, hum_msb, hum_lsb
    uint8_t buffer[8];
    readRegs(REG_DATA, buffer, 8);

    // Combine bytes into raw ADC integers (20-bit and 16-bit)
    int32_t adc_P = (buffer[0] << 12) | (buffer[1] << 4) | (buffer[2] >> 4);
    int32_t adc_T = (buffer[3] << 12) | (buffer[4] << 4) | (buffer[5] >> 4);
    int32_t adc_H = (buffer[6] << 8) | buffer[7];

    // Calculate Compensated Values
    // Note: MUST calculate Temp first because it updates 't_fine'
    data.temperature = compensateTemp(adc_T);
    data.pressure = compensatePressure(adc_P);
    data.humidity = compensateHumidity(adc_H);

    return data;
}

// --- Low Level I2C ---

void BME280::writeReg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    write(i2c_fd, buf, 2);
}

void BME280::readRegs(uint8_t reg, uint8_t* buffer, int length) {
    write(i2c_fd, &reg, 1); // Ask for register
    read(i2c_fd, buffer, length); // Read response
}

// --- Calibration Loading ---
bool BME280::readCalibrationData() {
    uint8_t buf[26];
    uint8_t bufH[7];

    // Read Temp/Pressure calib (0x88 - 0xA1)
    readRegs(0x88, buf, 26);
    calib.dig_T1 = (buf[1] << 8) | buf[0];
    calib.dig_T2 = (int16_t)((buf[3] << 8) | buf[2]);
    calib.dig_T3 = (int16_t)((buf[5] << 8) | buf[4]);
    calib.dig_P1 = (buf[7] << 8) | buf[6];
    calib.dig_P2 = (int16_t)((buf[9] << 8) | buf[8]);
    calib.dig_P3 = (int16_t)((buf[11] << 8) | buf[10]);
    calib.dig_P4 = (int16_t)((buf[13] << 8) | buf[12]);
    calib.dig_P5 = (int16_t)((buf[15] << 8) | buf[14]);
    calib.dig_P6 = (int16_t)((buf[17] << 8) | buf[16]);
    calib.dig_P7 = (int16_t)((buf[19] << 8) | buf[18]);
    calib.dig_P8 = (int16_t)((buf[21] << 8) | buf[20]);
    calib.dig_P9 = (int16_t)((buf[23] << 8) | buf[22]);

    // Read Humidity calib
    readRegs(0xA1, &calib.dig_H1, 1);
    readRegs(0xE1, bufH, 7);
    calib.dig_H2 = (int16_t)((bufH[1] << 8) | bufH[0]);
    calib.dig_H3 = bufH[2];
    calib.dig_H4 = (int16_t)((bufH[3] << 4) | (bufH[4] & 0x0F));
    calib.dig_H5 = (int16_t)((bufH[5] << 4) | (bufH[4] >> 4));
    calib.dig_H6 = (int8_t)bufH[6];

    return true;
}

// --- Compensation Formulas (From Bosch Datasheet) ---

float BME280::compensateTemp(int32_t adc_T) {
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1))) * ((int32_t)calib.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >> 12) * ((int32_t)calib.dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8; // Returns temp in 100ths of DegC (e.g. 2500 = 25.00C)
    // We convert to float:
    return ((t_fine * 5 + 128) >> 8) / 100.0f;
}

float BME280::compensatePressure(int32_t adc_P) {
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib.dig_P3) >> 8) + ((var1 * (int64_t)calib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib.dig_P1) >> 33;
    if (var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)calib.dig_P7) << 4);
    return (float)p / 256.0f / 100.0f; // hPa
}

float BME280::compensateHumidity(int32_t adc_H) {
    int32_t v_x1_u32r;
    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)calib.dig_H4) << 20) - (((int32_t)calib.dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)calib.dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)calib.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)calib.dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)calib.dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
    return (float)(v_x1_u32r >> 12) / 1024.0f;
}

} // namespace horus