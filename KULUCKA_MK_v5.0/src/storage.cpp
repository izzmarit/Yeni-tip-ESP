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

bool Storage::queueSave() {
    if (!_isInitialized) {
        return false;
    }
    
    // Değişikliği kuyrukla
    _pendingChanges++;
    _saveScheduled = true;
    
    // Eğer çok fazla değişiklik varsa hemen kaydet
    if (_pendingChanges >= EEPROM_MAX_CHANGES) {
        return saveSettings();
    }
    
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
    if (_isInitialized && _pendingChanges > 0) {
        saveSettings();
    }
}

unsigned long Storage::getTimeSinceLastSave() const {
    return millis() - _lastSaveTime;
}

bool Storage::saveSettings() {
    if (!_isInitialized) {
        return false;
    }
    
    // Doğrulama kodunu ayarla
    _data.validationCode = VALIDATION_CODE;
    
    // Veriyi EEPROM'a yaz
    _writeToEEPROM(0, _data);
    
    // Watchdog'u besle (EEPROM işlemleri zaman alabilir)
    esp_task_wdt_reset();
    
    // Değişiklikleri kaydet
    bool result = EEPROM.commit();
    
    if (result) {
        // Kuyruk değişkenlerini sıfırla
        _pendingChanges = 0;
        _saveScheduled = false;
        _lastSaveTime = millis();
    }
    
    return result;
}

bool Storage::loadSettings() {
    StorageData tempData;
    
    // EEPROM'dan veriyi oku
    _readFromEEPROM(0, tempData);
    
    // Watchdog'u besle (EEPROM işlemleri zaman alabilir)
    esp_task_wdt_reset();
    
    // Doğrulama kodunu kontrol et
    if (tempData.validationCode == VALIDATION_CODE) {
        // Veri geçerli, verileri yükle
        _data = tempData;
        _lastSaveTime = millis(); // İlk yükleme zamanını kaydet
        return true;
    }
    
    // Veri geçerli değil, varsayılan değerlere dön
    return false;
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