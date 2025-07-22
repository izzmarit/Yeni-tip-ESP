/**
 * @file fram_manager.h
 * @brief MB85RC256V FRAM yönetim modülü
 * @version 1.0
 */

#ifndef FRAM_MANAGER_H
#define FRAM_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "i2c_manager.h"

class FRAMManager {
public:
    FRAMManager();
    
    // FRAM'i başlat
    bool begin();
    
    // FRAM'e veri yaz
    bool write(uint16_t address, uint8_t data);
    bool write(uint16_t address, const uint8_t* data, size_t length);
    
    // FRAM'den veri oku
    uint8_t read(uint16_t address);
    bool read(uint16_t address, uint8_t* data, size_t length);
    
    // Toplu veri işlemleri
    template<typename T>
    bool writeObject(uint16_t address, const T& object) {
        return write(address, (const uint8_t*)&object, sizeof(T));
    }
    
    template<typename T>
    bool readObject(uint16_t address, T& object) {
        return read(address, (uint8_t*)&object, sizeof(T));
    }
    
    // FRAM'i sıfırla
    void clear();
    
    // FRAM bağlantısını test et
    bool testConnection();
    
    // Write protect kontrolü
    void setWriteProtect(bool enable);
    
    // FRAM boyutunu al
    uint32_t getSize() const { return FRAM_SIZE; }
    
private:
    uint8_t _deviceAddress;
    bool _isInitialized;
    int _wpPin;
    
    // I2C haberleşme fonksiyonları
    bool _writeI2C(uint16_t memAddress, const uint8_t* data, size_t length);
    bool _readI2C(uint16_t memAddress, uint8_t* data, size_t length);
    void _beginTransmission(uint16_t address);
};

#endif // FRAM_MANAGER_H