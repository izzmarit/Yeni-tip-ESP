/**
 * @file sensors.cpp
 * @brief SHT31 sıcaklık ve nem sensörleri yönetimi uygulaması
 * @version 1.0
 */

#include "sensors.h"

Sensors::Sensors() {
    _tempCalibration1 = 0.0;
    _tempCalibration2 = 0.0;
    _humidCalibration1 = 0.0;
    _humidCalibration2 = 0.0;
    
    _lastTemp1 = 0.0;
    _lastTemp2 = 0.0;
    _lastHumid1 = 0.0;
    _lastHumid2 = 0.0;
    
    _sensor1Working = false;
    _sensor2Working = false;
    
    _historyIndex = 0;
    _lastHistoryUpdate = 0;
    
    // Geçmiş veri başlangıç değerleri
    for (int i = 0; i < 5; i++) {
        _tempHistory[i] = 0.0;
        _humidHistory[i] = 0.0;
    }
}

bool Sensors::begin() {
    Wire.begin(I2C_SDA, I2C_SCL);
    
    // Alt sensör başlatma
    if (!_sht31_1.begin(SHT31_ADDR_1)) {
        _sensor1Working = false;
    } else {
        _sensor1Working = true;
    }
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    // Üst sensör başlatma
    if (!_sht31_2.begin(SHT31_ADDR_2)) {
        _sensor2Working = false;
    } else {
        _sensor2Working = true;
    }
    
    // İlk okuma
    _readSensorData();
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    // En az bir sensör çalışıyorsa başarılı
    return (_sensor1Working || _sensor2Working);
}

void Sensors::_readSensorData() {
    // Alt sensör verilerini oku
    if (_sensor1Working) {
        float t1 = _sht31_1.readTemperature();
        float h1 = _sht31_1.readHumidity();
        
        // Watchdog besleme - I2C iletişimi sırasında
        esp_task_wdt_reset();
        
        // NaN kontrolü
        if (!isnan(t1) && !isnan(h1)) {
            _lastTemp1 = t1 + _tempCalibration1;
            _lastHumid1 = h1 + _humidCalibration1;
        } else {
            _sensor1Working = false;
        }
    }
    
    // Üst sensör verilerini oku
    if (_sensor2Working) {
        float t2 = _sht31_2.readTemperature();
        float h2 = _sht31_2.readHumidity();
        
        // Watchdog besleme - I2C iletişimi sırasında
        esp_task_wdt_reset();
        
        // NaN kontrolü
        if (!isnan(t2) && !isnan(h2)) {
            _lastTemp2 = t2 + _tempCalibration2;
            _lastHumid2 = h2 + _humidCalibration2;
        } else {
            _sensor2Working = false;
        }
    }
    
    // Geçmiş verileri güncelle
    _updateHistory();
}

void Sensors::_updateHistory() {
    // Her dakikada bir geçmiş verileri güncelle
    unsigned long currentMillis = millis();
    if (currentMillis - _lastHistoryUpdate >= 60000) { // 1 dakika
        _lastHistoryUpdate = currentMillis;
        
        _tempHistory[_historyIndex] = readTemperature();
        _humidHistory[_historyIndex] = readHumidity();
        
        _historyIndex = (_historyIndex + 1) % 5; // 0-4 arası döngü
    }
}

float Sensors::readTemperature() {
    // Sensör verilerini oku
    _readSensorData();
    
    // Her iki sensör de çalışıyorsa ortalama değer döndür
    if (_sensor1Working && _sensor2Working) {
        return (_lastTemp1 + _lastTemp2) / 2.0;
    }
    // Sadece alt sensör çalışıyorsa onun değerini döndür
    else if (_sensor1Working) {
        return _lastTemp1;
    }
    // Sadece üst sensör çalışıyorsa onun değerini döndür
    else if (_sensor2Working) {
        return _lastTemp2;
    }
    // Hiçbir sensör çalışmıyorsa -999 döndür (hata durumu)
    else {
        return -999.0;
    }
}

float Sensors::readHumidity() {
    // Sensör verilerini oku
    _readSensorData();
    
    // Her iki sensör de çalışıyorsa ortalama değer döndür
    if (_sensor1Working && _sensor2Working) {
        return (_lastHumid1 + _lastHumid2) / 2.0;
    }
    // Sadece alt sensör çalışıyorsa onun değerini döndür
    else if (_sensor1Working) {
        return _lastHumid1;
    }
    // Sadece üst sensör çalışıyorsa onun değerini döndür
    else if (_sensor2Working) {
        return _lastHumid2;
    }
    // Hiçbir sensör çalışmıyorsa -999 döndür (hata durumu)
    else {
        return -999.0;
    }
}

bool Sensors::areSensorsWorking() {
    return (_sensor1Working || _sensor2Working);
}

bool Sensors::isSensorWorking(uint8_t sensorIndex) {
    if (sensorIndex == 0) {
        return _sensor1Working;
    } else if (sensorIndex == 1) {
        return _sensor2Working;
    }
    return false;
}

void Sensors::setTemperatureCalibration(float calibValue1, float calibValue2) {
    _tempCalibration1 = calibValue1;
    _tempCalibration2 = calibValue2;
}

void Sensors::setHumidityCalibration(float calibValue1, float calibValue2) {
    _humidCalibration1 = calibValue1;
    _humidCalibration2 = calibValue2;
}

float Sensors::getTemperatureCalibration(uint8_t sensorIndex) {
    if (sensorIndex == 0) {
        return _tempCalibration1;
    } else if (sensorIndex == 1) {
        return _tempCalibration2;
    }
    return 0.0;
}

float Sensors::getHumidityCalibration(uint8_t sensorIndex) {
    if (sensorIndex == 0) {
        return _humidCalibration1;
    } else if (sensorIndex == 1) {
        return _humidCalibration2;
    }
    return 0.0;
}

float Sensors::getLast5MinAvgTemperature() {
    float sum = 0.0;
    uint8_t count = 0;
    
    for (int i = 0; i < 5; i++) {
        if (_tempHistory[i] != 0.0) {
            sum += _tempHistory[i];
            count++;
        }
    }
    
    if (count > 0) {
        return sum / count;
    } else {
        return readTemperature(); // Geçmiş veri yoksa mevcut değeri döndür
    }
}

float Sensors::getLast5MinAvgHumidity() {
    float sum = 0.0;
    uint8_t count = 0;
    
    for (int i = 0; i < 5; i++) {
        if (_humidHistory[i] != 0.0) {
            sum += _humidHistory[i];
            count++;
        }
    }
    
    if (count > 0) {
        return sum / count;
    } else {
        return readHumidity(); // Geçmiş veri yoksa mevcut değeri döndür
    }
}