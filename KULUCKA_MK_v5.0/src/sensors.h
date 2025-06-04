/**
 * @file sensors.h
 * @brief SHT31 sıcaklık ve nem sensörleri yönetimi
 * @version 1.0
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include "config.h"

class Sensors {
public:
    // Yapılandırıcı
    Sensors();
    
    // Sensörleri başlat
    bool begin();
    
    // Sıcaklık değerlerini oku
    float readTemperature();
    
    // Nem değerlerini oku
    float readHumidity();

    // Belirli bir sensörün sıcaklık değerini oku (0 veya 1)
    float readTemperature(uint8_t sensorIndex);
    
    // Belirli bir sensörün nem değerini oku (0 veya 1)
    float readHumidity(uint8_t sensorIndex);
    
    // Her iki sensörün de çalışıp çalışmadığını kontrol et
    bool areSensorsWorking();
    
    // Belirli bir sensörün çalışıp çalışmadığını kontrol et
    bool isSensorWorking(uint8_t sensorIndex);
    
    // Sensör kalibrasyonunu ayarla
    void setTemperatureCalibration(float calibValue1, float calibValue2);
    void setHumidityCalibration(float calibValue1, float calibValue2);
    
    // Kalibrasyon değerlerini al
    float getTemperatureCalibration(uint8_t sensorIndex);
    float getHumidityCalibration(uint8_t sensorIndex);
    
    // Sıcaklık geçmiş verileri
    float getLast5MinAvgTemperature();
    
    // Nem geçmiş verileri
    float getLast5MinAvgHumidity();
    
    // I2C hata sayısını al
    int getI2CErrorCount() const;

private:
    Adafruit_SHT31 _sht31_1; // Alt sensör
    Adafruit_SHT31 _sht31_2; // Üst sensör
    
    float _tempCalibration1;
    float _tempCalibration2;
    float _humidCalibration1;
    float _humidCalibration2;
    
    float _lastTemp1;
    float _lastTemp2;
    float _lastHumid1;
    float _lastHumid2;
    
    bool _sensor1Working;
    bool _sensor2Working;
    
    float _tempHistory[5]; // Son 5 dakikalık sıcaklık ortalamaları
    float _humidHistory[5]; // Son 5 dakikalık nem ortalamaları
    uint8_t _historyIndex;
    unsigned long _lastHistoryUpdate;
    
    // I2C hata sayacı
    int _i2cErrorCount;
    
    // Sensörleri başlat
    bool _initSensors();
    
    // Sensörleri yeniden başlat
    void _restartSensors();
    
    // Sensörlerden ham veri oku
    void _readSensorData();
    
    // Geçmiş verileri güncelle
    void _updateHistory();
};

#endif // SENSORS_H