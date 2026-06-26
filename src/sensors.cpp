#include "sensors.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <Arduino.h>

#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" {
#include "bmi270.h"
}

namespace
{
constexpr i2c_port_t I2C_PORT = I2C_NUM_0;
constexpr gpio_num_t I2C_SDA = GPIO_NUM_18;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_17;
constexpr uint32_t I2C_CLOCK_HZ = 50000;
constexpr uint8_t MS5611_ADDRESS = 0x77;
constexpr uint8_t BMI270_ADDRESS = 0x69;
constexpr uint8_t PCF8563_ADDRESS = 0x51;
constexpr uint8_t GT911_ADDRESS = 0x5D;
constexpr TickType_t I2C_TIMEOUT = pdMS_TO_TICKS(1000);
constexpr TickType_t I2C_SCAN_TIMEOUT = pdMS_TO_TICKS(50);
constexpr char TAG[] = "sensors";

struct Bmi270Interface
{
    uint8_t address;
};

esp_err_t i2cWrite(uint8_t address, const uint8_t *data, size_t length)
{
    return i2c_master_write_to_device(I2C_PORT, address, data, length, I2C_TIMEOUT);
}

esp_err_t i2cReadRegister(uint8_t address, uint8_t reg, uint8_t *data, size_t length)
{
    return i2c_master_write_read_device(I2C_PORT, address, &reg, 1, data, length, I2C_TIMEOUT);
}

esp_err_t i2cReadCommand(uint8_t address, uint8_t command, uint8_t *data, size_t length)
{
    const esp_err_t writeResult = i2cWrite(address, &command, 1);
    if (writeResult != ESP_OK)
    {
        return writeResult;
    }
    return i2c_master_read_from_device(I2C_PORT, address, data, length, I2C_TIMEOUT);
}

esp_err_t i2cReadRegister16(uint8_t address, uint16_t reg, uint8_t *data, size_t length)
{
    const uint8_t registerAddress[] = {
        static_cast<uint8_t>(reg >> 8U),
        static_cast<uint8_t>(reg),
    };
    return i2c_master_write_read_device(
        I2C_PORT, address, registerAddress, sizeof(registerAddress), data, length, I2C_TIMEOUT);
}

esp_err_t i2cWriteRegister16(uint8_t address, uint16_t reg, uint8_t value)
{
    const uint8_t data[] = {
        static_cast<uint8_t>(reg >> 8U),
        static_cast<uint8_t>(reg),
        value,
    };
    return i2cWrite(address, data, sizeof(data));
}

uint8_t bcdToDecimal(uint8_t value)
{
    return static_cast<uint8_t>((value >> 4U) * 10U + (value & 0x0FU));
}

uint8_t decimalToBcd(uint8_t value)
{
    return static_cast<uint8_t>(((value / 10U) << 4U) | (value % 10U));
}

esp_err_t i2cProbe(uint8_t address)
{
    i2c_cmd_handle_t command = i2c_cmd_link_create();
    if (command == nullptr)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = i2c_master_start(command);
    if (result == ESP_OK)
    {
        result = i2c_master_write_byte(command, static_cast<uint8_t>(address << 1U), true);
    }
    if (result == ESP_OK)
    {
        result = i2c_master_stop(command);
    }
    if (result == ESP_OK)
    {
        result = i2c_master_cmd_begin(I2C_PORT, command, I2C_SCAN_TIMEOUT);
    }

    i2c_cmd_link_delete(command);
    return result;
}

BMI2_INTF_RETURN_TYPE bmi270Read(uint8_t reg, uint8_t *data, uint32_t length, void *interfacePointer)
{
    const auto *interface = static_cast<Bmi270Interface *>(interfacePointer);
    return i2cReadRegister(interface->address, reg, data, length) == ESP_OK
               ? BMI2_INTF_RET_SUCCESS
               : BMI2_E_COM_FAIL;
}

BMI2_INTF_RETURN_TYPE bmi270Write(uint8_t reg, const uint8_t *data, uint32_t length, void *interfacePointer)
{
    if (length > 32)
    {
        return BMI2_E_COM_FAIL;
    }

    const auto *interface = static_cast<Bmi270Interface *>(interfacePointer);
    uint8_t buffer[33];
    buffer[0] = reg;
    std::memcpy(buffer + 1, data, length);
    return i2cWrite(interface->address, buffer, length + 1) == ESP_OK
               ? BMI2_INTF_RET_SUCCESS
               : BMI2_E_COM_FAIL;
}

void bmi270Delay(uint32_t periodUs, void *)
{
    esp_rom_delay_us(periodUs);
}
} // namespace

namespace sensors
{
bool initI2c()
{
    const i2c_config_t config{
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = I2C_CLOCK_HZ},
        .clk_flags = 0,
    };

    const bool initialized = i2c_param_config(I2C_PORT, &config) == ESP_OK &&
                             i2c_driver_install(I2C_PORT, config.mode, 0, 0, 0) == ESP_OK;
    if (initialized)
    {
        ESP_LOGI(TAG, "I2C initialized: SDA=GPIO%d, SCL=GPIO%d, frequency=%lu Hz",
                 I2C_SDA, I2C_SCL, static_cast<unsigned long>(I2C_CLOCK_HZ));
        ESP_LOGI(TAG, "I2C idle levels: SDA=%d, SCL=%d",
                 gpio_get_level(I2C_SDA), gpio_get_level(I2C_SCL));
    }
    return initialized;
}

std::vector<uint8_t> scanI2cBus()
{
    std::vector<uint8_t> devices;
    for (uint8_t address = 0x03; address <= 0x77; ++address)
    {
        if (i2cProbe(address) == ESP_OK)
        {
            devices.push_back(address);
        }
    }
    return devices;
}

bool readBarometerIdentity(BarometerIdentity &identity)
{
    identity = {};
    const bool boschIdRead =
        i2cReadRegister(MS5611_ADDRESS, 0xD0, &identity.boschChipId, 1) == ESP_OK;
    const bool dpsIdRead =
        i2cReadRegister(MS5611_ADDRESS, 0x0D, &identity.dps310ProductId, 1) == ESP_OK;

    bool promRead = true;
    for (uint8_t index = 0; index < 7; ++index)
    {
        uint8_t data[2];
        const uint8_t registerAddress = static_cast<uint8_t>(0xA0 + index * 2);
        if (i2cReadCommand(MS5611_ADDRESS, registerAddress, data, sizeof(data)) != ESP_OK)
        {
            promRead = false;
            break;
        }
        identity.ms5611Prom[index] = static_cast<uint16_t>((data[0] << 8U) | data[1]);
    }

    return boschIdRead || dpsIdRead || promRead;
}

bool readPcf8563(DateTime &dateTime)
{
    uint8_t registers[7];
    if (i2cReadRegister(PCF8563_ADDRESS, 0x02, registers, sizeof(registers)) != ESP_OK)
    {
        return false;
    }

    dateTime.valid = (registers[0] & 0x80U) == 0;
    dateTime.second = bcdToDecimal(registers[0] & 0x7FU);
    dateTime.minute = bcdToDecimal(registers[1] & 0x7FU);
    dateTime.hour = bcdToDecimal(registers[2] & 0x3FU);
    dateTime.day = bcdToDecimal(registers[3] & 0x3FU);
    dateTime.weekday = registers[4] & 0x07U;
    dateTime.month = bcdToDecimal(registers[5] & 0x1FU);
    dateTime.year = static_cast<uint16_t>(((registers[5] & 0x80U) ? 1900U : 2000U) +
                                          bcdToDecimal(registers[6]));
    return true;
}

bool setPcf8563(const DateTime &dateTime)
{
    if (dateTime.year < 1900 || dateTime.year > 2099 ||
        dateTime.month < 1 || dateTime.month > 12 ||
        dateTime.day < 1 || dateTime.day > 31 ||
        dateTime.weekday > 6 ||
        dateTime.hour > 23 || dateTime.minute > 59 || dateTime.second > 59)
    {
        return false;
    }

    const uint8_t registers[] = {
        0x02,
        decimalToBcd(dateTime.second),
        decimalToBcd(dateTime.minute),
        decimalToBcd(dateTime.hour),
        decimalToBcd(dateTime.day),
        dateTime.weekday,
        static_cast<uint8_t>(decimalToBcd(dateTime.month) |
                             (dateTime.year < 2000 ? 0x80U : 0x00U)),
        decimalToBcd(static_cast<uint8_t>(dateTime.year % 100U)),
    };
    return i2cWrite(PCF8563_ADDRESS, registers, sizeof(registers)) == ESP_OK;
}

bool readGt911ProductId(char productId[5])
{
    uint8_t data[4];
    if (i2cReadRegister16(GT911_ADDRESS, 0x8140, data, sizeof(data)) != ESP_OK)
    {
        return false;
    }

    for (size_t i = 0; i < sizeof(data); ++i)
    {
        productId[i] = static_cast<char>(data[i]);
    }
    productId[4] = '\0';
    return true;
}

bool readGt911Touches(std::vector<TouchPoint> &touches)
{
    uint8_t status;
    if (i2cReadRegister16(GT911_ADDRESS, 0x814E, &status, 1) != ESP_OK)
    {
        return false;
    }

    touches.clear();
    if ((status & 0x80U) == 0)
    {
        return true;
    }

    const uint8_t touchCount = status & 0x0FU;
    if (touchCount > 5)
    {
        i2cWriteRegister16(GT911_ADDRESS, 0x814E, 0);
        return false;
    }

    uint8_t data[8 * 5];
    const bool readOk =
        touchCount == 0 ||
        i2cReadRegister16(GT911_ADDRESS, 0x8150, data, touchCount * 8U) == ESP_OK;
    if (readOk)
    {
        for (uint8_t i = 0; i < touchCount; ++i)
        {
            const uint8_t *point = data + i * 8U;
            touches.push_back({
                .x = static_cast<uint16_t>(point[1] | (point[2] << 8U)),
                .y = static_cast<uint16_t>(point[3] | (point[4] << 8U)),
                .size = static_cast<uint16_t>(point[5] | (point[6] << 8U)),
            });
        }
    }

    return i2cWriteRegister16(GT911_ADDRESS, 0x814E, 0) == ESP_OK && readOk;
}

bool initMs5611(Ms5611 &sensor)
{
    Serial.printf("MS5611 init: address 0x%02X reset command\n", MS5611_ADDRESS);
    const uint8_t resetCommand = 0x1E;
    esp_err_t result = ESP_FAIL;
    for (int attempt = 0; attempt < 3 && result != ESP_OK; ++attempt)
    {
        result = i2cWrite(MS5611_ADDRESS, &resetCommand, 1);
        Serial.printf("MS5611 reset attempt %d: %s\n", attempt + 1, esp_err_to_name(result));
        if (result != ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MS5611 reset failed: %s", esp_err_to_name(result));
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    for (uint8_t index = 0; index < 7; ++index)
    {
        uint8_t data[2];
        const uint8_t registerAddress = static_cast<uint8_t>(0xA0 + index * 2);
        result = ESP_FAIL;
        for (int attempt = 0; attempt < 3 && result != ESP_OK; ++attempt)
        {
            result = i2cReadCommand(MS5611_ADDRESS, registerAddress, data, sizeof(data));
            Serial.printf("MS5611 PROM[%u] read attempt %d at 0x%02X: %s\n",
                          index, attempt + 1, registerAddress, esp_err_to_name(result));
        }
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "MS5611 PROM read %u failed: %s", index, esp_err_to_name(result));
            return false;
        }

        sensor.calibration[index] = static_cast<uint16_t>((data[0] << 8) | data[1]);
        Serial.printf("MS5611 PROM[%u] = 0x%04X (%u)\n",
                      index, sensor.calibration[index], sensor.calibration[index]);
        if (index > 0 && sensor.calibration[index] == 0)
        {
            ESP_LOGE(TAG, "MS5611 PROM word %u is zero", index);
            return false;
        }
    }

    return true;
}

bool readMs5611Adc(uint8_t conversionCommand, uint32_t &value)
{
    const esp_err_t startResult = i2cWrite(MS5611_ADDRESS, &conversionCommand, 1);
    if (startResult != ESP_OK)
    {
        Serial.printf("MS5611 ADC conversion command 0x%02X failed: %s\n",
                      conversionCommand, esp_err_to_name(startResult));
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t data[3];
    const esp_err_t readResult = i2cReadCommand(MS5611_ADDRESS, 0x00, data, sizeof(data));
    if (readResult != ESP_OK)
    {
        Serial.printf("MS5611 ADC read for command 0x%02X failed: %s\n",
                      conversionCommand, esp_err_to_name(readResult));
        return false;
    }

    value = (static_cast<uint32_t>(data[0]) << 16) |
            (static_cast<uint32_t>(data[1]) << 8) |
            data[2];
    Serial.printf("MS5611 ADC command 0x%02X raw=%lu bytes=%02X %02X %02X\n",
                  conversionCommand, static_cast<unsigned long>(value), data[0], data[1], data[2]);
    return true;
}

bool readMs5611(const Ms5611 &sensor, float &temperatureC, float &pressureHpa)
{
    uint32_t pressureRaw;
    uint32_t temperatureRaw;

    if (!readMs5611Adc(0x48, pressureRaw) || !readMs5611Adc(0x58, temperatureRaw))
    {
        Serial.println("MS5611 read failed: pressure or temperature ADC read failed");
        return false;
    }

    const int64_t dT = static_cast<int64_t>(temperatureRaw) -
                       static_cast<int64_t>(sensor.calibration[5]) * 256;
    int64_t temperature = 2000 + dT * sensor.calibration[6] / 8388608;

    int64_t offset = static_cast<int64_t>(sensor.calibration[2]) * 131072 +
                     dT * sensor.calibration[4] / 64;
    int64_t sensitivity = static_cast<int64_t>(sensor.calibration[1]) * 65536 +
                          dT * sensor.calibration[3] / 128;

    if (temperature < 2000)
    {
        const int64_t temperatureDelta = temperature - 2000;
        const int64_t temperature2 = dT * dT / 2147483648LL;
        int64_t offset2 = 5 * temperatureDelta * temperatureDelta / 2;
        int64_t sensitivity2 = 5 * temperatureDelta * temperatureDelta / 4;

        if (temperature < -1500)
        {
            const int64_t coldDelta = temperature + 1500;
            offset2 += 7 * coldDelta * coldDelta;
            sensitivity2 += 11 * coldDelta * coldDelta / 2;
        }

        temperature -= temperature2;
        offset -= offset2;
        sensitivity -= sensitivity2;
    }

    const int64_t pressure = ((static_cast<int64_t>(pressureRaw) * sensitivity / 2097152) -
                              offset) /
                             32768;

    temperatureC = temperature / 100.0F;
    pressureHpa = pressure / 100.0F;
    Serial.printf("MS5611 calculated: pressureRaw=%lu temperatureRaw=%lu temp=%.2f C pressure=%.2f hPa\n",
                  static_cast<unsigned long>(pressureRaw),
                  static_cast<unsigned long>(temperatureRaw),
                  temperatureC,
                  pressureHpa);
    return true;
}

bool initBmi270(bmi2_dev &sensor, float &accelScale, float &gyroScale)
{
    sensor.intf = BMI2_I2C_INTF;
    // store interface internally
    static Bmi270Interface interface{BMI270_ADDRESS};
    sensor.intf_ptr = &interface;
    sensor.read = bmi270Read;
    sensor.write = bmi270Write;
    sensor.delay_us = bmi270Delay;
    sensor.read_write_len = 32;

    int8_t result = bmi270_init(&sensor);
    if (result != BMI2_OK)
    {
        ESP_LOGE(TAG, "BMI270 initialization failed: error %d", result);
        return false;
    }

    const uint8_t sensorsArray[] = {BMI2_ACCEL, BMI2_GYRO};
    result = bmi2_sensor_enable(sensorsArray, 2, &sensor);
    if (result != BMI2_OK)
    {
        ESP_LOGE(TAG, "BMI270 sensor enable failed: error %d", result);
        return false;
    }

    bmi2_sens_config configs[2]{};
    configs[0].type = BMI2_ACCEL;
    configs[1].type = BMI2_GYRO;
    result = bmi2_get_sensor_config(configs, 2, &sensor);
    if (result != BMI2_OK)
    {
        ESP_LOGE(TAG, "BMI270 config read failed: error %d", result);
        return false;
    }

    constexpr float accelRanges[] = {2.0F, 4.0F, 8.0F, 16.0F};
    constexpr float gyroRanges[] = {2000.0F, 1000.0F, 500.0F, 250.0F, 125.0F};
    accelScale = accelRanges[configs[0].cfg.acc.range] / 32768.0F;
    gyroScale = gyroRanges[configs[1].cfg.gyr.range] / 32768.0F;
    return true;
}

} // namespace sensors
