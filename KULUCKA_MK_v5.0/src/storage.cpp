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

    // Yeni thread safety değişkenleri
    _isWriting = false;
    _lockStartTime = 0;
    _retryCount = 0;
    _dataCorrupted = false;
    _lastValidationCode = 0;
    
    // Varsayılan değerleri yükle
    loadDefaults();
}

bool Storage::begin() {
    // EEPROM'u başlat
    EEPROM.begin(EEPROM_SIZE);
    
    // Mevcut ayarları yükle
    if (!loadSettings()) {
        // Ayarlar yüklenemezse varsayılan değerleri kullan
        loadDefaults();
        // Varsayılan değerleri kaydet
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
    // Belirli bir süre geçtiyse ve değişiklik varsa kaydet
    if ((currentTime - _lastSaveTime >= EEPROM_WRITE_DELAY) && _pendingChanges > 0) {
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

// Template metotların implementasyonu
template<typename T>
void Storage::_writeToEEPROM(int address, const T& value) {
    const byte* p = (const byte*)(const void*)&value;
    for (unsigned int i = 0; i < sizeof(value); i++) {
        EEPROM.write(address + i, *p++);
    }
}

template<typename T>
void Storage::_readFromEEPROM(int address, T& value) {
    byte* p = (byte*)(void*)&value;
    for (unsigned int i = 0; i < sizeof(value); i++) {
        *p++ = EEPROM.read(address + i);
    }
}

// Storage.cpp dosyasının sonuna eklenecek thread-safe fonksiyonlar

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
    // Backup data structure (basit implementasyon)
    StorageData backupData = _data;
    
    // Backup EEPROM alanına yaz (EEPROM_SIZE/2 offset ile)
    _writeToEEPROM(EEPROM_SIZE / 2, backupData);
    
    bool result = EEPROM.commit();
    if (result) {
        Serial.println("Storage: Backup oluşturuldu");
    } else {
        Serial.println("Storage: Backup oluşturma hatası!");
    }
    
    return result;
}

bool Storage::_restoreFromBackup() {
    StorageData backupData;
    
    // Backup alanından oku
    _readFromEEPROM(EEPROM_SIZE / 2, backupData);
    
    // Backup data validation
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
    
    // Lock almaya çalış
    if (!_acquireLock()) {
        return false;
    }
    
    bool result = false;
    
    // Data validation
    if (!_validateData()) {
        Serial.println("Storage: Data validation hatası!");
        _releaseLock();
        return false;
    }
    
    // Backup oluştur
    _createBackup();
    
    // Ana veriyi yaz
    _data.validationCode = VALIDATION_CODE;
    _writeToEEPROM(0, _data);
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    result = EEPROM.commit();
    
    if (result) {
        _pendingChanges = 0;
        _saveScheduled = false;
        _lastSaveTime = millis();
        _dataCorrupted = false;
        Serial.println("Storage: Thread-safe kayıt başarılı");
    } else {
        Serial.println("Storage: Kayıt hatası!");
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
    
    // Ana veriyi oku
    _readFromEEPROM(0, tempData);
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    // Data validation
    if (tempData.validationCode == VALIDATION_CODE) {
        _data = tempData;
        
        // Ek validation
        if (_validateData()) {
            _lastSaveTime = millis();
            _dataCorrupted = false;
            result = true;
            Serial.println("Storage: Ana veri başarıyla yüklendi");
        } else {
            Serial.println("Storage: Ana veri geçersiz, backup deneniyor...");
            result = _restoreFromBackup();
        }
    } else {
        Serial.println("Storage: Ana veri bozuk, backup deneniyor...");
        result = _restoreFromBackup();
    }
    
    if (!result) {
        Serial.println("Storage: Tüm veriler bozuk, varsayılan değerler yükleniyor");
        loadDefaults();
        result = true; // Varsayılan değerlerle devam et
    }
    
    _releaseLock();
    return result;
}

// Thread-safe queueSave implementasyonu
bool Storage::queueSave() {
    if (!_isInitialized) {
        return false;
    }
    
    // Eğer yazma işlemi devam ediyorsa bekle
    if (_isWriting) {
        if (millis() - _lockStartTime > STORAGE_LOCK_TIMEOUT) {
            Serial.println("Storage: queueSave - yazma işlemi timeout!");
            return false;
        }
    }
    
    _pendingChanges++;
    _saveScheduled = true;
    
    // Kritik değişiklik sayısı daha düşük bir eşikte hemen kaydet
    if (_pendingChanges >= 3) { // 10 yerine 3
        Serial.println("Storage: Kritik değişiklik sayısına ulaşıldı, anında kayıt yapılıyor");
        return saveSettings();
    }
    
    return true;
}

