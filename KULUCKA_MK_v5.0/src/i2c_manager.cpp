/**
 * @file i2c_manager.cpp
 * @brief I2C bus yönetimi implementasyonu
 * @version 1.0
 */

#include "i2c_manager.h"

I2CManager::I2CManager() {
    _i2cMutex = NULL;
    _initialized = false;
    _busErrors = 0;
    _lastResetTime = 0;
}

I2CManager::~I2CManager() {
    if (_i2cMutex != NULL) {
        vSemaphoreDelete(_i2cMutex);
    }
}

bool I2CManager::begin() {
    if (_initialized) {
        return true;
    }
    
    _i2cMutex = xSemaphoreCreateMutex();
    if (_i2cMutex == NULL) {
        Serial.println("I2C Manager: Mutex oluşturulamadı!");
        return false;
    }
    
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000); // 100kHz standart hız
    
    _initialized = true;
    Serial.println("I2C Manager: Başlatıldı");
    
    // Bus taraması yap
    scanBus();
    
    return true;
}

bool I2CManager::takeBus(uint32_t timeoutMs) {
    if (!_initialized || _i2cMutex == NULL) {
        return false;
    }
    
    if (xSemaphoreTake(_i2cMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        return true;
    }
    
    _busErrors++;
    Serial.println("I2C Manager: Bus alınamadı! Hata sayısı: " + String(_busErrors));
    
    // Çok fazla hata varsa bus'ı resetle
    if (_busErrors > 10) {
        resetBus();
    }
    
    return false;
}

void I2CManager::releaseBus() {
    if (_initialized && _i2cMutex != NULL) {
        xSemaphoreGive(_i2cMutex);
    }
}

void I2CManager::scanBus() {
    Serial.println("I2C Bus Taraması:");
    int deviceCount = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("I2C cihaz bulundu: 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);
            
            // Bilinen cihazları tanımla
            switch (address) {
                case 0x44:
                    Serial.print(" (SHT31 Sensör 1)");
                    break;
                case 0x45:
                    Serial.print(" (SHT31 Sensör 2)");
                    break;
                case 0x50:
                    Serial.print(" (FRAM)");
                    break;
                case 0x68:
                    Serial.print(" (RTC)");
                    break;
            }
            Serial.println();
            deviceCount++;
        }
        delay(1);
    }
    
    Serial.println("Toplam " + String(deviceCount) + " cihaz bulundu");
}

bool I2CManager::isDeviceReady(uint8_t address) {
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0);
}

void I2CManager::resetBus() {
    unsigned long currentTime = millis();
    
    if (currentTime - _lastResetTime < 5000) {
        return; // 5 saniyede bir defadan fazla reset yapma
    }
    
    Serial.println("I2C Manager: Bus reset yapılıyor...");
    
    Wire.end();
    delay(100);
    
    // SCL ve SDA pinlerini manuel olarak HIGH yap
    pinMode(I2C_SCL, OUTPUT);
    pinMode(I2C_SDA, OUTPUT);
    digitalWrite(I2C_SCL, HIGH);
    digitalWrite(I2C_SDA, HIGH);
    delay(100);
    
    // I2C'yi yeniden başlat
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);
    
    _busErrors = 0;
    _lastResetTime = currentTime;
    
    Serial.println("I2C Manager: Bus reset tamamlandı");
}