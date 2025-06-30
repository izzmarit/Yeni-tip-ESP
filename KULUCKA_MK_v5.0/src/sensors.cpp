/**
 * @file sensors.cpp
 * @brief SHT31 sıcaklık ve nem sensörleri yönetimi uygulaması
 * @version 1.0
 */

#include "sensors.h"
#include "watchdog_manager.h"

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
    _i2cErrorCount = 0;
    
    // Geçmiş veri başlangıç değerleri
    for (int i = 0; i < 5; i++) {
        _tempHistory[i] = 0.0;
        _humidHistory[i] = 0.0;
    }
}

bool Sensors::begin() {
    // I2C başlatmayı dene, başarısız olursa tekrar deneyecek
    for (int attempt = 0; attempt < 3; attempt++) {
        Wire.begin(I2C_SDA, I2C_SCL);
        delay(50);
        
        bool sensorsStarted = _initSensors();
        
        if (sensorsStarted) {
            return true;
        }
        
        Serial.print("I2C başlatma hatası, tekrar deneniyor (");
        Serial.print(attempt + 1);
        Serial.println("/3)");
        
        // I2C hatası durumunda bus'ı sıfırla
        Wire.end();
        delay(100);
    }
    
    // En son deneme
    Wire.begin(I2C_SDA, I2C_SCL);
    return _initSensors();
}

bool Sensors::_initSensors() {
    bool sensorsOk = false;
    
    // Alt sensör başlatma
    if (!_sht31_1.begin(SHT31_ADDR_1)) {
        Serial.println("Alt sensör (SHT31-1) başlatılamadı!");
        _sensor1Working = false;
    } else {
        _sensor1Working = true;
        sensorsOk = true;
    }
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    // Üst sensör başlatma
    if (!_sht31_2.begin(SHT31_ADDR_2)) {
        Serial.println("Üst sensör (SHT31-2) başlatılamadı!");
        _sensor2Working = false;
    } else {
        _sensor2Working = true;
        sensorsOk = true;
    }
    
    // İlk okuma
    _readSensorData();
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    // En az bir sensör çalışıyorsa başarılı
    return sensorsOk;
}

void Sensors::_readSensorData() {
    // Watchdog yöneticisini dahil et
    extern WatchdogManager watchdogManager;
    
    // DÜZELTME: Error threshold kontrolü başta
    if (_i2cErrorCount > SENSOR_MAX_CONSECUTIVE_ERRORS) {
        static unsigned long lastRestartAttempt = 0;
        if (millis() - lastRestartAttempt > 30000) { // 30 saniyede bir deneme
            lastRestartAttempt = millis();
            Serial.println("I2C hata sayısı çok yüksek (" + String(_i2cErrorCount) + 
                          "), sensörleri yeniden başlatma deneniyor...");
            _restartSensors();
            _i2cErrorCount = 0;
        }
        return; // Hata durumunda erken çıkış
    }
    
    // Alt sensör verilerini oku - İYİLEŞTİRİLMİŞ
    if (_sensor1Working) {
        float t1, h1;
        bool success = false;
        
        // DÜZELTME: Enhanced retry logic with exponential backoff
        for (int retryCount = 0; retryCount < 3 && !success; retryCount++) {
            watchdogManager.feed(); // Her deneme öncesi watchdog besleme
            
            // DÜZELTME: Progressive delay
            if (retryCount > 0) {
                delay(50 * retryCount); // 50ms, 100ms delays
            }
            
            t1 = _sht31_1.readTemperature();
            h1 = _sht31_1.readHumidity();
            
            // NaN ve range kontrolü
            if (!isnan(t1) && !isnan(h1) && 
                t1 > -40.0 && t1 < 85.0 && h1 >= 0.0 && h1 <= 100.0) {
                
                _lastTemp1 = t1 + _tempCalibration1;
                _lastHumid1 = h1 + _humidCalibration1;
                success = true;
                
                // DÜZELTME: Başarılı okuma sonrası hata sayacını azalt
                if (_i2cErrorCount > 0) {
                    _i2cErrorCount--;
                }
                
            } else {
                _i2cErrorCount++;
                
                Serial.println("Alt sensör okuma hatası " + String(retryCount + 1) + 
                              " - T:" + String(t1) + " H:" + String(h1) + 
                              " Total Errors: " + String(_i2cErrorCount));
                
                // DÜZELTME: Sadece son denemede I2C reset yap
                if (retryCount == 2) {
                    Serial.println("I2C bus reset deneniyor...");
                    Wire.end();
                    delay(100);
                    Wire.begin(I2C_SDA, I2C_SCL);
                    delay(50);
                    
                    // Sensörü yeniden başlat
                    if (!_sht31_1.begin(SHT31_ADDR_1)) {
                        Serial.println("Alt sensör yeniden başlatma hatası!");
                        _sensor1Working = false;
                    }
                }
            }
        }
        
        // DÜZELTME: Sürekli başarısızlık kontrolü
        if (!success) {
            static int consecutiveFailures1 = 0;
            consecutiveFailures1++;
            
            if (consecutiveFailures1 >= 5) {
                Serial.println("Alt sensör (SHT31-1) kalıcı okuma hatası! Devre dışı bırakılıyor.");
                _sensor1Working = false;
                consecutiveFailures1 = 0;
            }
        } else {
            // Başarılı okuma sonrası consecutive failure sıfırla
            static int consecutiveFailures1 = 0;
            consecutiveFailures1 = 0;
        }
    }
    
    // Üst sensör için aynı mantık (kod tekrarını önlemek için aynı yapı)
    if (_sensor2Working) {
        float t2, h2;
        bool success = false;
        
        for (int retryCount = 0; retryCount < 3 && !success; retryCount++) {
            watchdogManager.feed();
            
            if (retryCount > 0) {
                delay(50 * retryCount);
            }
            
            t2 = _sht31_2.readTemperature();
            h2 = _sht31_2.readHumidity();
            
            if (!isnan(t2) && !isnan(h2) && 
                t2 > -40.0 && t2 < 85.0 && h2 >= 0.0 && h2 <= 100.0) {
                
                _lastTemp2 = t2 + _tempCalibration2;
                _lastHumid2 = h2 + _humidCalibration2;
                success = true;
                
                if (_i2cErrorCount > 0) {
                    _i2cErrorCount--;
                }
                
            } else {
                _i2cErrorCount++;
                
                Serial.println("Üst sensör okuma hatası " + String(retryCount + 1) + 
                              " - T:" + String(t2) + " H:" + String(h2) + 
                              " Total Errors: " + String(_i2cErrorCount));
                
                if (retryCount == 2) {
                    Serial.println("I2C bus reset deneniyor...");
                    Wire.end();
                    delay(100);
                    Wire.begin(I2C_SDA, I2C_SCL);
                    delay(50);
                    
                    if (!_sht31_2.begin(SHT31_ADDR_2)) {
                        Serial.println("Üst sensör yeniden başlatma hatası!");
                        _sensor2Working = false;
                    }
                }
            }
        }
        
        if (!success) {
            static int consecutiveFailures2 = 0;
            consecutiveFailures2++;
            
            if (consecutiveFailures2 >= 5) {
                Serial.println("Üst sensör (SHT31-2) kalıcı okuma hatası! Devre dışı bırakılıyor.");
                _sensor2Working = false;
                consecutiveFailures2 = 0;
            }
        } else {
            static int consecutiveFailures2 = 0;
            consecutiveFailures2 = 0;
        }
    }
    
    // Kritik durum: Her iki sensör de çalışmıyorsa
    if (!_sensor1Working && !_sensor2Working) {
        watchdogManager.setEmergencyMode(true);
        
        Serial.println("KRİTİK: Tüm sensörler arızalı! Acil durum modu aktif.");
        
        // DÜZELTME: Sensörleri yeniden başlatmaya çalış
        static unsigned long lastRecoveryAttempt = 0;
        if (millis() - lastRecoveryAttempt > 60000) { // 60 saniyede bir deneme
            lastRecoveryAttempt = millis();
            
            Serial.println("Sensör recovery deneniyor...");
            _restartSensors();
            
            // Recovery başarılıysa acil durumu kapat
            if (_sensor1Working || _sensor2Working) {
                watchdogManager.setEmergencyMode(false);
                Serial.println("Sensör recovery başarılı!");
                _i2cErrorCount = 0; // Error count sıfırla
            }
        }
    } else if (watchdogManager.getCurrentState() == WD_EMERGENCY) {
        // En az bir sensör çalışıyorsa acil durum modunu kapat
        watchdogManager.setEmergencyMode(false);
        Serial.println("Sensör durumu düzeldi, acil durum modu kapatıldı");
    }
    
    // Geçmiş verileri güncelle
    _updateHistory();
}

void Sensors::_restartSensors() {
    extern WatchdogManager watchdogManager;
    
    Serial.println("Sensör yeniden başlatma işlemi başlatılıyor...");
    
    // I2C bus'ı tamamen sıfırla
    Wire.end();
    delay(200);
    
    // Watchdog besleme
    watchdogManager.feed();
    
    // I2C'yi yeniden başlat
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(100);
    
    // Sensör 1'i yeniden başlat
    _sensor1Working = false;
    if (_sht31_1.begin(SHT31_ADDR_1)) {
        _sensor1Working = true;
        Serial.println("Alt sensör (SHT31-1) başarıyla yeniden başlatıldı");
    } else {
        Serial.println("Alt sensör (SHT31-1) yeniden başlatılamadı!");
    }
    
    // Watchdog besleme
    watchdogManager.feed();
    delay(100);
    
    // Sensör 2'yi yeniden başlat
    _sensor2Working = false;
    if (_sht31_2.begin(SHT31_ADDR_2)) {
        _sensor2Working = true;
        Serial.println("Üst sensör (SHT31-2) başarıyla yeniden başlatıldı");
    } else {
        Serial.println("Üst sensör (SHT31-2) yeniden başlatılamadı!");
    }
    
    // İlk okuma testi
    if (_sensor1Working || _sensor2Working) {
        Serial.println("En az bir sensör çalışır durumda, ilk okuma yapılıyor...");
        delay(500); // Sensörlerin stabilize olması için bekle
        
        // İlk test okuma
        _readSensorData();
    }
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

// Yeni fonksiyonlar - Sensörlerin ayrı değerlerini al
float Sensors::readTemperature(uint8_t sensorIndex) {
    _readSensorData();
    
    if (sensorIndex == 0 && _sensor1Working) {
        return _lastTemp1;
    } else if (sensorIndex == 1 && _sensor2Working) {
        return _lastTemp2;
    }
    
    return -999.0; // Sensör çalışmıyorsa hata değeri
}

float Sensors::readHumidity(uint8_t sensorIndex) {
    _readSensorData();
    
    if (sensorIndex == 0 && _sensor1Working) {
        return _lastHumid1;
    } else if (sensorIndex == 1 && _sensor2Working) {
        return _lastHumid2;
    }
    
    return -999.0; // Sensör çalışmıyorsa hata değeri
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

int Sensors::getI2CErrorCount() const {
    return _i2cErrorCount;
}

bool Sensors::hasValidReading() const {
    // En az bir sensörden valid reading var mı?
    return _sensor1Working || _sensor2Working;
}

void Sensors::setTemperatureCalibrationSingle(uint8_t sensorIndex, float calibValue) {
    if (sensorIndex == 0) {
        _tempCalibration1 = calibValue;
        Serial.println("Sensör 1 sıcaklık kalibrasyonu ayarlandı: " + String(calibValue));
    } else if (sensorIndex == 1) {
        _tempCalibration2 = calibValue;
        Serial.println("Sensör 2 sıcaklık kalibrasyonu ayarlandı: " + String(calibValue));
    }
}

void Sensors::setHumidityCalibrationSingle(uint8_t sensorIndex, float calibValue) {
    if (sensorIndex == 0) {
        _humidCalibration1 = calibValue;
        Serial.println("Sensör 1 nem kalibrasyonu ayarlandı: " + String(calibValue));
    } else if (sensorIndex == 1) {
        _humidCalibration2 = calibValue;
        Serial.println("Sensör 2 nem kalibrasyonu ayarlandı: " + String(calibValue));
    }
}

void Sensors::setTemperatureCalibration(float calibValue1, float calibValue2) {
    _tempCalibration1 = calibValue1;
    _tempCalibration2 = calibValue2;
    Serial.println("Sıcaklık kalibrasyonları ayarlandı - S1: " + String(calibValue1) + 
                   " S2: " + String(calibValue2));
}

void Sensors::setHumidityCalibration(float calibValue1, float calibValue2) {
    _humidCalibration1 = calibValue1;
    _humidCalibration2 = calibValue2;
    Serial.println("Nem kalibrasyonları ayarlandı - S1: " + String(calibValue1) + 
                   " S2: " + String(calibValue2));
}