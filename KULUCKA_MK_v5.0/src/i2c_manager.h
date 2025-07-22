/**
 * @file i2c_manager.h
 * @brief I2C bus yönetimi ve semafor kontrolü
 * @version 1.0
 */

#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

class I2CManager {
public:
    static I2CManager& getInstance() {
        static I2CManager instance;
        return instance;
    }
    
    bool begin();
    bool takeBus(uint32_t timeoutMs = 100);
    void releaseBus();
    void scanBus();
    bool isDeviceReady(uint8_t address);
    void resetBus();
    
private:
    I2CManager();
    ~I2CManager();
    I2CManager(const I2CManager&) = delete;
    I2CManager& operator=(const I2CManager&) = delete;
    
    SemaphoreHandle_t _i2cMutex;
    bool _initialized;
    uint32_t _busErrors;
    unsigned long _lastResetTime;
};

#define I2C_MANAGER I2CManager::getInstance()

#endif