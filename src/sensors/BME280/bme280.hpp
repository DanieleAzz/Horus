#pragma once

#include <string>
#include <cstdint>

namespace horus {

struct BME280Data {
    float temperature; // Celsius
    float humidity;    // %
    float pressure;    // hPa
};

class BME280 {
public:
    BME280(uint8_t i2cAddress = 0x76, int busId = 1);
    ~BME280();

    // Initialize sensor (soft reset, read calibration data, set config)
    bool init();

    // Read the current values
    BME280Data readAll();

private:
    int i2c_fd;
    int busId;
    uint8_t deviceAddress;

    // Calibration data (Trim parameters from datasheet)
    // The sensor stores these internally to correct its own raw data.
    struct CalibData {
        uint16_t dig_T1;
        int16_t  dig_T2, dig_T3;
        uint8_t  dig_H1, dig_H3;
        int16_t  dig_H2, dig_H4, dig_H5;
        int8_t   dig_H6;
        uint16_t dig_P1;
        int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    } calib;

    // Internal helper methods
    bool readCalibrationData();
    void writeReg(uint8_t reg, uint8_t value);
    void readRegs(uint8_t reg, uint8_t* buffer, int length);
    
    // The Bosch compensation logic
    int32_t t_fine; // Intermediate temperature value used for pressure/humidity
    float compensateTemp(int32_t adc_T);
    float compensatePressure(int32_t adc_P);
    float compensateHumidity(int32_t adc_H);
};

} // namespace horus