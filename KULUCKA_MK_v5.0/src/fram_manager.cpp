/**
 * @file fram_manager.cpp
 * @brief MB85RC256V FRAM yönetim modülü implementasyonu
 * @version 1.0
 */

#include "fram_manager.h"
#include "i2c_manager.h"

FRAMManager::FRAMManager() {
    _deviceAddress = FRAM_ADDRESS;
    _isInitialized = false;
    _wpPin = FRAM_WRITE_PROTECT_PIN;
}

bool FRAMManager::begin() {
    // Write protect pini varsa yapılandır
    if (_wpPin >= 0) {
        pinMode(_wpPin, OUTPUT);
        digitalWrite(_wpPin, LOW); // Write protect devre dışı
    }
    
    // I2C bus'ı al
    if (!I2C_MANAGER.takeBus(1000)) {
        Serial.println("FRAM: I2C bus alınamadı!");
        return false;
    }
    
    // FRAM bağlantısını test et
    bool connectionOk = testConnection();
    
    // I2C bus'ı serbest bırak
    I2C_MANAGER.releaseBus();
    
    if (!connectionOk) {
        Serial.println("FRAM: Bağlantı hatası!");
        return false;
    }
    
    // FRAM kimlik doğrulaması
    uint16_t verificationCode;
    readObject(0, verificationCode);
    
    if (verificationCode != FRAM_VERIFICATION_CODE) {
        Serial.println("FRAM: İlk kullanım, başlatılıyor...");
        clear();
        writeObject(0, (uint16_t)FRAM_VERIFICATION_CODE);
    }
    
    _isInitialized = true;
    Serial.println("FRAM: Başarıyla başlatıldı (32KB)");
    return true;
}

bool FRAMManager::write(uint16_t address, uint8_t data) {
    return write(address, &data, 1);
}

bool FRAMManager::write(uint16_t address, const uint8_t* data, size_t length) {
    if (!_isInitialized) {
        return false;
    }
    
    // Adres sınır kontrolü
    if (address + length > FRAM_SIZE) {
        Serial.println("FRAM: Yazma adresi sınır dışı!");
        return false;
    }
    
    return _writeI2C(address, data, length);
}

uint8_t FRAMManager::read(uint16_t address) {
    uint8_t data;
    read(address, &data, 1);
    return data;
}

bool FRAMManager::read(uint16_t address, uint8_t* data, size_t length) {
    if (!_isInitialized) {
        return false;
    }
    
    // Adres sınır kontrolü
    if (address + length > FRAM_SIZE) {
        Serial.println("FRAM: Okuma adresi sınır dışı!");
        return false;
    }
    
    return _readI2C(address, data, length);
}

void FRAMManager::clear() {
    Serial.println("FRAM: Bellek temizleniyor...");
    
    uint8_t zeroBuffer[32];
    memset(zeroBuffer, 0, sizeof(zeroBuffer));
    
    // 32 byte'lık bloklar halinde temizle
    for (uint16_t addr = 0; addr < FRAM_SIZE; addr += sizeof(zeroBuffer)) {
        write(addr, zeroBuffer, sizeof(zeroBuffer));
        
        // Her 1KB'da bir ilerleme göster
        if (addr % 1024 == 0) {
            Serial.print(".");
            esp_task_wdt_reset();
        }
    }
    
    Serial.println("\nFRAM: Bellek temizlendi");
}

bool FRAMManager::testConnection() {
    // Test verisi yaz ve oku
    const uint16_t testAddress = FRAM_SIZE - 4;
    const uint32_t testPattern = 0xDEADBEEF;
    uint32_t readValue;
    
    // Test verisini yaz
    if (!writeObject(testAddress, testPattern)) {
        return false;
    }
    
    // Test verisini oku
    if (!readObject(testAddress, readValue)) {
        return false;
    }
    
    // Doğrulama
    return (readValue == testPattern);
}

void FRAMManager::setWriteProtect(bool enable) {
    if (_wpPin >= 0) {
        digitalWrite(_wpPin, enable ? HIGH : LOW);
    }
}

// _writeI2C fonksiyonunu güncelle
bool FRAMManager::_writeI2C(uint16_t memAddress, const uint8_t* data, size_t length) {
    // I2C bus'ı al
    if (!I2C_MANAGER.takeBus(500)) {
        Serial.println("FRAM: I2C bus alınamadı!");
        return false;
    }
    
    bool result = true;
    const size_t maxChunkSize = 30;
    size_t bytesWritten = 0;
    
    while (bytesWritten < length && result) {
        size_t chunkSize = min(maxChunkSize, length - bytesWritten);
        
        Wire.beginTransmission(_deviceAddress);
        Wire.write((uint8_t)(memAddress >> 8));
        Wire.write((uint8_t)(memAddress & 0xFF));
        
        for (size_t i = 0; i < chunkSize; i++) {
            Wire.write(data[bytesWritten + i]);
        }
        
        if (Wire.endTransmission() != 0) {
            Serial.println("FRAM: I2C yazma hatası!");
            result = false;
        }
        
        memAddress += chunkSize;
        bytesWritten += chunkSize;
        delayMicroseconds(5);
    }
    
    // I2C bus'ı serbest bırak
    I2C_MANAGER.releaseBus();
    
    return result;
}

// _readI2C fonksiyonunu güncelle
bool FRAMManager::_readI2C(uint16_t memAddress, uint8_t* data, size_t length) {
    // I2C bus'ı al
    if (!I2C_MANAGER.takeBus(500)) {
        Serial.println("FRAM: I2C bus alınamadı!");
        return false;
    }
    
    bool result = true;
    
    Wire.beginTransmission(_deviceAddress);
    Wire.write((uint8_t)(memAddress >> 8));
    Wire.write((uint8_t)(memAddress & 0xFF));
    
    if (Wire.endTransmission(false) != 0) {
        Serial.println("FRAM: I2C adres gönderme hatası!");
        result = false;
    } else {
        size_t bytesRead = 0;
        while (bytesRead < length && result) {
            size_t chunkSize = min((size_t)32, length - bytesRead);
            
            Wire.requestFrom(_deviceAddress, (uint8_t)chunkSize);
            
            size_t available = Wire.available();
            if (available != chunkSize) {
                Serial.println("FRAM: Beklenen veri alınamadı!");
                result = false;
            } else {
                for (size_t i = 0; i < chunkSize; i++) {
                    data[bytesRead++] = Wire.read();
                }
            }
        }
    }
    
    // I2C bus'ı serbest bırak
    I2C_MANAGER.releaseBus();
    
    return result;
}

void FRAMManager::_beginTransmission(uint16_t address) {
    Wire.beginTransmission(_deviceAddress);
    Wire.write((uint8_t)(address >> 8));
    Wire.write((uint8_t)(address & 0xFF));
}