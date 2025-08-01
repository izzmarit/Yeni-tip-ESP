/**
 * @file storage.cpp
 * @brief Ayarlar ve durumları saklama modülü uygulaması
 * @version 1.2
 */

#include "storage.h"

Storage::Storage() {
    _isInitialized = false;
    _lastSaveTime = 0;
    _pendingChanges = 0;
    _saveScheduled = false;

    _isWriting = false;
    _lockStartTime = 0;
    _retryCount = 0;
    _dataCorrupted = false;
    _lastValidationCode = 0;
    
    // Storage tipini belirle
#if USE_FRAM
    _storageType = STORAGE_TYPE_FRAM;
#else
    _storageType = STORAGE_TYPE_EEPROM;
#endif
    
    loadDefaults();
}

bool Storage::begin() {
#if USE_FRAM
    // I2C Manager'ı başlat
    if (!I2C_MANAGER.begin()) {
        Serial.println("Storage: I2C Manager başlatma hatası!");
        _storageType = STORAGE_TYPE_EEPROM;
        EEPROM.begin(EEPROM_SIZE);
        return true; // EEPROM ile devam et
    }
    
    // FRAM'i başlat
    if (!_fram.begin()) {
        Serial.println("Storage: FRAM başlatma hatası, EEPROM'a geçiliyor");
        _storageType = STORAGE_TYPE_EEPROM;
        EEPROM.begin(EEPROM_SIZE);
    } else {
        Serial.println("Storage: FRAM kullanılıyor (32KB)");
    }
#else
    // EEPROM'u başlat
    EEPROM.begin(EEPROM_SIZE);
    _storageType = STORAGE_TYPE_EEPROM;
#endif
    
    // Mevcut ayarları yükle
    if (!loadSettings()) {
        loadDefaults();
        saveSettings();
    }
    
    _isInitialized = true;
    return true;
}

void Storage::processQueue() {
    if (!_isInitialized || !_saveScheduled) {
        return;
    }
    
    unsigned long currentTime = millis();
    
    // Kritik parametre değişikliği varsa hemen kaydet
    static unsigned long lastCriticalCheck = 0;
    if (currentTime - lastCriticalCheck > 1000) { // Her saniye kontrol et
        lastCriticalCheck = currentTime;
        
        // PID, sıcaklık, nem gibi kritik parametreler değiştiyse
        if (_pendingChanges > 0 && _hasCriticalChanges) {
            Serial.println("Storage: Kritik değişiklik tespit edildi, hemen kaydediliyor");
            saveSettings();
            _hasCriticalChanges = false;
            return;
        }
    }
    
    // Normal kayıt zamanlaması
    if ((currentTime - _lastSaveTime >= EEPROM_WRITE_DELAY) && _pendingChanges > 0) {
        saveSettings();
    }
    
    // Maksimum değişiklik sayısına ulaşıldıysa
    if (_pendingChanges >= EEPROM_MAX_CHANGES) {
        Serial.println("Storage: Maksimum değişiklik sayısına ulaşıldı, kaydediliyor");
        saveSettings();
    }
}

void Storage::saveStateNow() {
    if (!_isInitialized) {
        Serial.println("Storage: Başlatılmamış, kayıt yapılamıyor!");
        return;
    }
    
    // Watchdog besleme - kritik işlem başlıyor
    esp_task_wdt_reset();
    
    // Bekleyen değişiklik sayısını kontrol et
    if (_pendingChanges > 0) {
        Serial.println("Storage: Kritik kayıt başlatılıyor (" + String(_pendingChanges) + " değişiklik)");
        
        // Thread-safe kayıt işlemi
        bool result = saveSettings();
        
        if (result) {
            Serial.println("Storage: Kritik kayıt başarılı");
        } else {
            Serial.println("Storage: KRİTİK HATA - Kayıt başarısız!");
            
            // Kayıt başarısız olursa backup'tan restore etmeyi dene
            if (_restoreFromBackup()) {
                Serial.println("Storage: Backup'tan geri yükleme başarılı");
            }
        }
    } else {
        Serial.println("Storage: Değişiklik yok, kayıt atlandı");
    }
    
    // Watchdog besleme - kritik işlem bitti
    esp_task_wdt_reset();
}

unsigned long Storage::getTimeSinceLastSave() const {
    return millis() - _lastSaveTime;
}


void Storage::loadDefaults() {
    // Varsayılan değerleri ayarla
    _data.incubationType = INCUBATION_CHICKEN;
    _data.manualDevTemp = 37.5;
    _data.manualHatchTemp = 37.0;
    _data.manualDevHumid = 60;
    _data.manualHatchHumid = 70;
    _data.manualDevDays = 18;
    _data.manualHatchDays = 3;
    
    // YENİ: Varsayılan hedef değerler
    _data.targetTemperature = 37.5;
    _data.targetHumidity = 60;
    
    _data.isIncubationRunning = false;
    _data.startTimeUnix = 0;
    
    _data.pidKp = PID_KP;
    _data.pidKi = PID_KI;
    _data.pidKd = PID_KD;
    _data.pidMode = 0;       // YENİ: Varsayılan olarak PID kapalı
    
    _data.motorWaitTime = DEFAULT_MOTOR_WAIT_TIME;
    _data.motorRunTime = DEFAULT_MOTOR_RUN_TIME;
    
    _data.tempCalibration1 = 0.0;
    _data.tempCalibration2 = 0.0;
    _data.humidCalibration1 = 0.0;
    _data.humidCalibration2 = 0.0;
    
    _data.tempLowAlarm = DEFAULT_TEMP_LOW_ALARM;
    _data.tempHighAlarm = DEFAULT_TEMP_HIGH_ALARM;
    _data.humidLowAlarm = DEFAULT_HUMID_LOW_ALARM;
    _data.humidHighAlarm = DEFAULT_HUMID_HIGH_ALARM;
    _data.alarmsEnabled = true;  // Varsayılan olarak alarmlar etkin

    // Motor zamanlama varsayılan değerleri - YENİ EKLENECEK
    _data.motorLastActionTime = 0;
    _data.motorTimingState = 0;  // WAITING
    _data.motorElapsedTime = 0;
    
    // WiFi ayarları
    strncpy(_data.wifiSSID, AP_SSID, sizeof(_data.wifiSSID));
    strncpy(_data.wifiPassword, AP_PASS, sizeof(_data.wifiPassword));
    _data.wifiEnabled = true;
    _data.wifiMode = WIFI_CONN_MODE_AP; // Varsayılan olarak AP modu
    
    // Station modu ayarları (boş)
    memset(_data.stationSSID, 0, sizeof(_data.stationSSID));
    memset(_data.stationPassword, 0, sizeof(_data.stationPassword));
    
    _data.validationCode = VALIDATION_CODE;
}

// Yeni getter/setter implementasyonları:
uint8_t Storage::getPidMode() const {
    return _data.pidMode;
}

void Storage::setPidMode(uint8_t mode) {
    _data.pidMode = mode;
    
    // DÜZELTME: Aynı mantık
    if (_storageType == STORAGE_TYPE_FRAM) {
        _saveCriticalData();
    } else {
        _pendingChanges = EEPROM_MAX_CHANGES;
        _hasCriticalChanges = true;
    }
    
    markCriticalChange();
}

uint8_t Storage::getIncubationType() const {
    return _data.incubationType;
}

void Storage::setIncubationType(uint8_t type) {
    _data.incubationType = type;
}

bool Storage::isIncubationRunning() const {
    return _data.isIncubationRunning;
}

void Storage::setIncubationRunning(bool running) {
    _data.isIncubationRunning = running;
}

DateTime Storage::getStartTime() const {
    // Unix timestamp'i DateTime nesnesine dönüştür
    return DateTime(_data.startTimeUnix);
}

void Storage::setStartTime(DateTime startTime) {
    // DateTime nesnesini Unix timestamp'e dönüştür
    _data.startTimeUnix = startTime.unixtime();
}

float Storage::getManualDevTemp() const {
    return _data.manualDevTemp;
}

void Storage::setManualDevTemp(float temp) {
    _data.manualDevTemp = temp;
}

float Storage::getManualHatchTemp() const {
    return _data.manualHatchTemp;
}

void Storage::setManualHatchTemp(float temp) {
    _data.manualHatchTemp = temp;
}

uint8_t Storage::getManualDevHumid() const {
    return _data.manualDevHumid;
}

void Storage::setManualDevHumid(uint8_t humid) {
    _data.manualDevHumid = humid;
}

uint8_t Storage::getManualHatchHumid() const {
    return _data.manualHatchHumid;
}

void Storage::setManualHatchHumid(uint8_t humid) {
    _data.manualHatchHumid = humid;
}

uint8_t Storage::getManualDevDays() const {
    return _data.manualDevDays;
}

void Storage::setManualDevDays(uint8_t days) {
    _data.manualDevDays = days;
}

uint8_t Storage::getManualHatchDays() const {
    return _data.manualHatchDays;
}

void Storage::setManualHatchDays(uint8_t days) {
    _data.manualHatchDays = days;
}

float Storage::getPidKp() const {
    return _data.pidKp;
}

void Storage::setPidKp(float kp) {
    _data.pidKp = kp;
}

float Storage::getPidKi() const {
    return _data.pidKi;
}

void Storage::setPidKi(float ki) {
    _data.pidKi = ki;
}

float Storage::getPidKd() const {
    return _data.pidKd;
}

void Storage::setPidKd(float kd) {
    _data.pidKd = kd;
}

uint32_t Storage::getMotorWaitTime() const {
    return _data.motorWaitTime;
}

void Storage::setMotorWaitTime(uint32_t minutes) {
    _data.motorWaitTime = minutes;
}

uint32_t Storage::getMotorRunTime() const {
    return _data.motorRunTime;
}

void Storage::setMotorRunTime(uint32_t seconds) {
    _data.motorRunTime = seconds;
}

float Storage::getTempCalibration(uint8_t sensorIndex) const {
    if (sensorIndex == 0) {
        return _data.tempCalibration1;
    } else if (sensorIndex == 1) {
        return _data.tempCalibration2;
    }
    return 0.0;
}

void Storage::setTempCalibration(uint8_t sensorIndex, float value) {
    if (sensorIndex == 0) {
        _data.tempCalibration1 = value;
    } else if (sensorIndex == 1) {
        _data.tempCalibration2 = value;
    }
}

float Storage::getHumidCalibration(uint8_t sensorIndex) const {
    if (sensorIndex == 0) {
        return _data.humidCalibration1;
    } else if (sensorIndex == 1) {
        return _data.humidCalibration2;
    }
    return 0.0;
}

void Storage::setHumidCalibration(uint8_t sensorIndex, float value) {
    if (sensorIndex == 0) {
        _data.humidCalibration1 = value;
    } else if (sensorIndex == 1) {
        _data.humidCalibration2 = value;
    }
}

float Storage::getTempLowAlarm() const {
    return _data.tempLowAlarm;
}

void Storage::setTempLowAlarm(float value) {
    _data.tempLowAlarm = value;
}

float Storage::getTempHighAlarm() const {
    return _data.tempHighAlarm;
}

void Storage::setTempHighAlarm(float value) {
    _data.tempHighAlarm = value;
}

float Storage::getHumidLowAlarm() const {
    return _data.humidLowAlarm;
}

void Storage::setHumidLowAlarm(float value) {
    _data.humidLowAlarm = value;
}

float Storage::getHumidHighAlarm() const {
    return _data.humidHighAlarm;
}

void Storage::setHumidHighAlarm(float value) {
    _data.humidHighAlarm = value;
}

bool Storage::areAlarmsEnabled() const {
    return _data.alarmsEnabled;
}

void Storage::setAlarmsEnabled(bool enabled) {
    _data.alarmsEnabled = enabled;
    
    // DÜZELTME: Aynı mantık
    if (_storageType == STORAGE_TYPE_FRAM) {
        _saveCriticalData();
    } else {
        _pendingChanges = EEPROM_MAX_CHANGES;
        _hasCriticalChanges = true;
    }
    
    markCriticalChange();
}

String Storage::getWifiSSID() const {
    return String(_data.wifiSSID);
}

void Storage::setWifiSSID(const String& ssid) {
    strncpy(_data.wifiSSID, ssid.c_str(), sizeof(_data.wifiSSID) - 1);
    _data.wifiSSID[sizeof(_data.wifiSSID) - 1] = '\0'; // Null terminatör eklenmesini sağla
}

String Storage::getWifiPassword() const {
    return String(_data.wifiPassword);
}

void Storage::setWifiPassword(const String& password) {
    strncpy(_data.wifiPassword, password.c_str(), sizeof(_data.wifiPassword) - 1);
    _data.wifiPassword[sizeof(_data.wifiPassword) - 1] = '\0'; // Null terminatör eklenmesini sağla
}

bool Storage::isWifiEnabled() const {
    return _data.wifiEnabled;
}

void Storage::setWifiEnabled(bool enabled) {
    _data.wifiEnabled = enabled;
}

WiFiConnectionMode Storage::getWifiMode() const {
    return _data.wifiMode;
}

void Storage::setWifiMode(WiFiConnectionMode mode) {
    _data.wifiMode = mode;
}

String Storage::getStationSSID() const {
    return String(_data.stationSSID);
}

void Storage::setStationSSID(const String& ssid) {
    strncpy(_data.stationSSID, ssid.c_str(), sizeof(_data.stationSSID) - 1);
    _data.stationSSID[sizeof(_data.stationSSID) - 1] = '\0'; // Null terminatör eklenmesini sağla
}

String Storage::getStationPassword() const {
    return String(_data.stationPassword);
}

void Storage::setStationPassword(const String& password) {
    strncpy(_data.stationPassword, password.c_str(), sizeof(_data.stationPassword) - 1);
    _data.stationPassword[sizeof(_data.stationPassword) - 1] = '\0'; // Null terminatör eklenmesini sağla
}

void Storage::getData(StorageData& data) const {
    data = _data;
}

void Storage::setData(const StorageData& data) {
    _data = data;
}

bool Storage::_acquireLock() {
    unsigned long startTime = millis();
    
    while (_isWriting) {
        if (millis() - startTime > STORAGE_LOCK_TIMEOUT) {
            Serial.println("Storage: Lock timeout!");
            return false;
        }
        delay(10);
        esp_task_wdt_reset(); // Watchdog besleme
    }
    
    _isWriting = true;
    _lockStartTime = millis();
    return true;
}

void Storage::_releaseLock() {
    _isWriting = false;
    _lockStartTime = 0;
}

bool Storage::isCriticalParameter(const String& paramName) const {
    // Kritik parametreler listesi
    static const char* criticalParams[] = {
        "targetTemp", "targetHumid", "incubationType", "isIncubationRunning",
        "wifiMode", "pidMode", "alarmsEnabled", nullptr
    };
    
    for (int i = 0; criticalParams[i] != nullptr; i++) {
        if (paramName == criticalParams[i]) {
            return true;
        }
    }
    return false;
}

float Storage::getTargetTemperature() const {
    return _data.targetTemperature;
}

void Storage::setTargetTemperature(float temp) {
    _data.targetTemperature = temp;
    
    // DÜZELTME: Hem FRAM hem EEPROM için çalışacak şekilde güncelle
    if (_storageType == STORAGE_TYPE_FRAM) {
        _saveCriticalData(); // FRAM'e hızlı yazım
    } else {
        // EEPROM için anında kayıt
        _pendingChanges = EEPROM_MAX_CHANGES; // Anında kayıt tetikle
        _hasCriticalChanges = true;
    }
    
    markCriticalChange();
}

float Storage::getTargetHumidity() const {
    return _data.targetHumidity;
}

void Storage::setTargetHumidity(float humid) {
    _data.targetHumidity = humid;
    
    // DÜZELTME: Aynı mantık
    if (_storageType == STORAGE_TYPE_FRAM) {
        _saveCriticalData();
    } else {
        _pendingChanges = EEPROM_MAX_CHANGES;
        _hasCriticalChanges = true;
    }
    
    markCriticalChange();
}

bool Storage::_validateData() const {
    // Basit data validation
    if (_data.validationCode != VALIDATION_CODE) {
        return false;
    }
    
    // Değer aralık kontrolleri
    if (_data.incubationType > INCUBATION_MANUAL) return false;
    if (_data.manualDevTemp < 20.0 || _data.manualDevTemp > 45.0) return false;
    if (_data.manualHatchTemp < 20.0 || _data.manualHatchTemp > 45.0) return false;
    if (_data.manualDevHumid < 30 || _data.manualDevHumid > 90) return false;
    if (_data.manualHatchHumid < 30 || _data.manualHatchHumid > 90) return false;
    
    return true;
}

bool Storage::_createBackup() {
    StorageData backupData = _data;
    
#if USE_FRAM
    if (_storageType == STORAGE_TYPE_FRAM) {
        // FRAM backup alanına yaz
        bool result = _fram.writeObject(FRAM_BACKUP_START, backupData);
        if (result) {
            Serial.println("Storage: FRAM backup oluşturuldu");
        } else {
            Serial.println("Storage: FRAM backup oluşturma hatası!");
        }
        return result;
    } else {
#endif
        // EEPROM backup alanına yaz
        _writeToEEPROM(EEPROM_SIZE / 2, backupData);
        bool result = EEPROM.commit();
        if (result) {
            Serial.println("Storage: EEPROM backup oluşturuldu");
        } else {
            Serial.println("Storage: EEPROM backup oluşturma hatası!");
        }
        return result;
#if USE_FRAM
    }
#endif
}

bool Storage::_restoreFromBackup() {
    StorageData backupData;
    
#if USE_FRAM
    if (_storageType == STORAGE_TYPE_FRAM) {
        // FRAM backup alanından oku
        if (!_fram.readObject(FRAM_BACKUP_START, backupData)) {
            Serial.println("Storage: FRAM backup okuma hatası!");
            return false;
        }
    } else {
#endif
        // EEPROM backup alanından oku
        _readFromEEPROM(EEPROM_SIZE / 2, backupData);
#if USE_FRAM
    }
#endif
    
    if (backupData.validationCode == VALIDATION_CODE) {
        _data = backupData;
        Serial.println("Storage: Backup'tan restore edildi");
        return true;
    }
    
    Serial.println("Storage: Backup geçersiz!");
    return false;
}

// Thread-safe saveSettings implementasyonu
bool Storage::saveSettings() {
    if (!_isInitialized) {
        return false;
    }
    
    if (!_acquireLock()) {
        return false;
    }
    
    bool result = false;
    
    if (!_validateData()) {
        Serial.println("Storage: Data validation hatası!");
        _releaseLock();
        return false;
    }
    
    if (!_createBackup()) {
        Serial.println("Storage: Backup oluşturma hatası!");
        _releaseLock();
        return false;
    }
    
    _data.validationCode = VALIDATION_CODE;
    _updateCRC(_data); // CRC hesapla ve güncelle
    
#if USE_FRAM
    if (_storageType == STORAGE_TYPE_FRAM) {
        // FRAM'e yaz
        result = _fram.writeObject(FRAM_DATA_START, _data);
        
        if (result) {
            Serial.println("Storage: FRAM'e yazma başarılı");
        } else {
            Serial.println("Storage: FRAM yazma hatası!");
        }
    } else {
#endif
        // EEPROM'a yaz
        _writeToEEPROM(0, _data);
        esp_task_wdt_reset();
        
        int commitAttempts = 0;
        while (commitAttempts < 3 && !result) {
            result = EEPROM.commit();
            if (!result) {
                Serial.println("Storage: EEPROM commit denemesi " + String(commitAttempts + 1) + " başarısız");
                delay(100);
                esp_task_wdt_reset();
            }
            commitAttempts++;
        }
#if USE_FRAM
    }
#endif
    
    if (result) {
        _pendingChanges = 0;
        _saveScheduled = false;
        _lastSaveTime = millis();
        _dataCorrupted = false;
        
        // Doğrulama
        delay(50);
        StorageData verifyData;
        
#if USE_FRAM
        if (_storageType == STORAGE_TYPE_FRAM) {
            _fram.readObject(FRAM_DATA_START, verifyData);
        } else {
#endif
            _readFromEEPROM(0, verifyData);
#if USE_FRAM
        }
#endif
        
        if (verifyData.validationCode != VALIDATION_CODE) {
            Serial.println("Storage: UYARI - Kayıt doğrulama hatası!");
            result = false;
        }
    } else {
        Serial.println("Storage: KRITIK - Veri kaydetme hatası!");
        _dataCorrupted = true;
    }
    
    _releaseLock();
    return result;
}

// Thread-safe loadSettings implementasyonu
bool Storage::loadSettings() {
    if (!_acquireLock()) {
        return false;
    }
    
    StorageData tempData;
    bool result = false;
    
#if USE_FRAM
    if (_storageType == STORAGE_TYPE_FRAM) {
        // Önce kritik verileri yüklemeyi dene
        bool criticalDataLoaded = _loadCriticalData();
        if (criticalDataLoaded) {
            Serial.println("Storage: Kritik veriler öncelikli olarak yüklendi");
        }
        
        // Ana veriyi oku
        if (_fram.readObject(FRAM_DATA_START, tempData)) {
            Serial.println("Storage: FRAM'den veri okundu");
        } else {
            Serial.println("Storage: FRAM okuma hatası!");
            _releaseLock();
            return false;
        }
    } else {
#endif
        // EEPROM'dan oku
        _readFromEEPROM(0, tempData);
#if USE_FRAM
    }
#endif
    
    esp_task_wdt_reset();
    
    if (tempData.validationCode == VALIDATION_CODE) {
        // CRC kontrolü
        if (!_verifyCRC(tempData)) {
            Serial.println("Storage: CRC doğrulama hatası!");
            result = _restoreFromBackup();
        } else {
            _data = tempData;
            
            // Kritik veriler zaten yüklendiyse, onları korumak için tekrar uygula
#if USE_FRAM
            if (_storageType == STORAGE_TYPE_FRAM) {
                _loadCriticalData(); // Kritik verileri tekrar yükle (üzerine yazma koruması)
            }
#endif
            
            if (_validateData()) {
                _lastSaveTime = millis();
                _dataCorrupted = false;
                result = true;
                Serial.println("Storage: Ana veri başarıyla yüklendi");
            } else {
                Serial.println("Storage: Ana veri geçersiz, backup deneniyor...");
                result = _restoreFromBackup();
            }
        }
    } else {
        Serial.println("Storage: Ana veri bozuk, backup deneniyor...");
        result = _restoreFromBackup();
    }
    
    if (!result) {
        Serial.println("Storage: Tüm veriler bozuk, varsayılan değerler yükleniyor");
        loadDefaults();
        result = true;
    }
    
    _releaseLock();
    return result;
}

// Thread-safe queueSave implementasyonu
bool Storage::queueSave() {
    if (!_isInitialized) {
        return false;
    }
    
    // KRİTİK: TÜM DEĞİŞİKLİKLER ANINDA KAYDEDİLECEK
    // Eski implementasyon yerine doğrudan saveStateNow() çağır
    Serial.println("Storage: Kritik değişiklik tespit edildi, anında kaydediliyor");
    return saveSettings();
}

// Motor zamanlama durumu getter/setter implementasyonları - YENİ EKLENECEK
uint32_t Storage::getMotorLastActionTime() const {
    return _data.motorLastActionTime;
}

void Storage::setMotorLastActionTime(uint32_t time) {
    _data.motorLastActionTime = time;
    queueSave();
}

uint8_t Storage::getMotorTimingState() const {
    return _data.motorTimingState;
}

void Storage::setMotorTimingState(uint8_t state) {
    _data.motorTimingState = state;
    queueSave();
}

uint32_t Storage::getMotorElapsedTime() const {
    return _data.motorElapsedTime;
}

void Storage::setMotorElapsedTime(uint32_t time) {
    _data.motorElapsedTime = time;
    queueSave();
}

// CRC32 hesaplama fonksiyonu
uint32_t Storage::_calculateCRC32(const uint8_t* data, size_t length) {
    const uint32_t polynomial = 0xEDB88320;
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return ~crc;
}

bool Storage::_verifyCRC(const StorageData& data) {
    // CRC alanı hariç tüm veriyi hesapla
    size_t dataSize = sizeof(StorageData) - sizeof(uint32_t) * 2; // CRC ve validation code hariç
    uint32_t calculatedCRC = _calculateCRC32((const uint8_t*)&data, dataSize);
    
    return (calculatedCRC == data.crc32);
}

void Storage::_updateCRC(StorageData& data) {
    size_t dataSize = sizeof(StorageData) - sizeof(uint32_t) * 2;
    data.crc32 = _calculateCRC32((const uint8_t*)&data, dataSize);
}

void Storage::_saveCriticalData() {
#if USE_FRAM
    if (_storageType != STORAGE_TYPE_FRAM) return;
    
    CriticalData critical;
    critical.targetTemp = _data.targetTemperature;
    critical.targetHumid = _data.targetHumidity;
    critical.incubationRunning = _data.isIncubationRunning;
    critical.pidMode = _data.pidMode;
    critical.alarmsEnabled = _data.alarmsEnabled;
    critical.timestamp = millis();
    
    // CRC16 hesapla
    critical.crc16 = _calculateCRC16((uint8_t*)&critical, 
                                     sizeof(CriticalData) - sizeof(uint16_t));
    
    // FRAM'e yaz (mutex kontrolü olmadan, çok hızlı)
    _fram.writeObject(FRAM_CRITICAL_START, critical);
    
    // Debug log
    static unsigned long lastCriticalSave = 0;
    if (millis() - lastCriticalSave > 10000) { // 10 saniyede bir log
        lastCriticalSave = millis();
        Serial.println("FRAM: Kritik veriler kaydedildi (sık güncelleme)");
    }
#endif
}

uint16_t Storage::_calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

bool Storage::_loadCriticalData() {
#if USE_FRAM
    if (_storageType != STORAGE_TYPE_FRAM) {
        return false;
    }
    
    // Kritik veri yapısını oku
    CriticalData critical;
    if (!_fram.readObject(FRAM_CRITICAL_START, critical)) {
        Serial.println("Storage: Kritik veri okuma hatası!");
        return false;
    }
    
    // CRC16 doğrulaması yap
    uint16_t calculatedCRC = _calculateCRC16((uint8_t*)&critical, 
                                             sizeof(CriticalData) - sizeof(uint16_t));
    
    if (calculatedCRC != critical.crc16) {
        Serial.println("Storage: Kritik veri CRC hatası! Hesaplanan: " + 
                      String(calculatedCRC, HEX) + " Okunan: " + String(critical.crc16, HEX));
        return false;
    }
    
    // Timestamp kontrolü - verinin çok eski olup olmadığını kontrol et
    unsigned long currentTime = millis();
    if (critical.timestamp > 0 && currentTime > 3600000) { // Sistem 1 saatten fazla çalışıyorsa
        unsigned long dataAge = currentTime - critical.timestamp;
        if (dataAge > 86400000) { // 24 saatten eski veri
            Serial.println("Storage: Kritik veri çok eski, göz ardı ediliyor");
            return false;
        }
    }
    
    // Kritik verileri ana veri yapısına uygula
    bool dataChanged = false;
    
    // Hedef sıcaklık kontrolü ve güncelleme
    if (critical.targetTemp >= TEMP_MIN && critical.targetTemp <= TEMP_MAX) {
        if (_data.targetTemperature != critical.targetTemp) {
            _data.targetTemperature = critical.targetTemp;
            dataChanged = true;
            Serial.println("Storage: Hedef sıcaklık yüklendi: " + String(critical.targetTemp));
        }
    }
    
    // Hedef nem kontrolü ve güncelleme
    if (critical.targetHumid >= HUMID_MIN && critical.targetHumid <= HUMID_MAX) {
        if (_data.targetHumidity != critical.targetHumid) {
            _data.targetHumidity = critical.targetHumid;
            dataChanged = true;
            Serial.println("Storage: Hedef nem yüklendi: " + String(critical.targetHumid));
        }
    }
    
    // Kuluçka durumu güncelleme
    if (_data.isIncubationRunning != critical.incubationRunning) {
        _data.isIncubationRunning = critical.incubationRunning;
        dataChanged = true;
        Serial.println("Storage: Kuluçka durumu yüklendi: " + 
                      String(critical.incubationRunning ? "Çalışıyor" : "Durmuş"));
    }
    
    // PID modu güncelleme
    if (critical.pidMode <= 2) { // Geçerli PID modu kontrolü
        if (_data.pidMode != critical.pidMode) {
            _data.pidMode = critical.pidMode;
            dataChanged = true;
            Serial.println("Storage: PID modu yüklendi: " + String(critical.pidMode));
        }
    }
    
    // Alarm durumu güncelleme
    if (_data.alarmsEnabled != critical.alarmsEnabled) {
        _data.alarmsEnabled = critical.alarmsEnabled;
        dataChanged = true;
        Serial.println("Storage: Alarm durumu yüklendi: " + 
                      String(critical.alarmsEnabled ? "Etkin" : "Devre dışı"));
    }
    
    if (dataChanged) {
        Serial.println("Storage: Kritik veriler başarıyla uygulandı");
        return true;
    } else {
        Serial.println("Storage: Kritik verilerde değişiklik yok");
        return false;
    }
#else
    return false;
#endif
}