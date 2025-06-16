/**
 * @file alarm.cpp
 * @brief Alarm yönetimi uygulaması
 * @version 1.2 - Alarm kapatma sorunu düzeltildi
 */

#include "alarm.h"

AlarmManager::AlarmManager() {
    _tempLowThreshold = DEFAULT_TEMP_LOW_ALARM;
    _tempHighThreshold = DEFAULT_TEMP_HIGH_ALARM;
    _humidLowThreshold = DEFAULT_HUMID_LOW_ALARM;
    _humidHighThreshold = DEFAULT_HUMID_HIGH_ALARM;
    
    _currentAlarm = ALARM_NONE;
    _isAlarmActive = false;
    _isSoundEnabled = true;
    _isAlarmDisabled = false;
    _areAlarmsEnabled = true;  // Varsayılan olarak alarmlar etkin
    
    _lastAlarmToggleTime = 0;
    _alarmLedState = false;
}

bool AlarmManager::begin() {
    // Alarm pinini çıkış olarak ayarla
    pinMode(ALARM_PIN, OUTPUT);
    digitalWrite(ALARM_PIN, LOW);
    
    return true;
}

void AlarmManager::setTempLowThreshold(float value) {
    _tempLowThreshold = value;
}

void AlarmManager::setTempHighThreshold(float value) {
    _tempHighThreshold = value;
}

void AlarmManager::setHumidLowThreshold(float value) {
    _humidLowThreshold = value;
}

void AlarmManager::setHumidHighThreshold(float value) {
    _humidHighThreshold = value;
}

float AlarmManager::getTempLowThreshold() const {
    return _tempLowThreshold;
}

float AlarmManager::getTempHighThreshold() const {
    return _tempHighThreshold;
}

float AlarmManager::getHumidLowThreshold() const {
    return _humidLowThreshold;
}

float AlarmManager::getHumidHighThreshold() const {
    return _humidHighThreshold;
}

void AlarmManager::setAlarmsEnabled(bool enabled) {
    _areAlarmsEnabled = enabled;
    
    if (!enabled) {
        // Alarmlar kapatılırken mevcut alarmı tamamen sıfırla - DÜZELTME
        _currentAlarm = ALARM_NONE;
        _isAlarmActive = false;
        digitalWrite(ALARM_PIN, LOW);
        Serial.println("Tüm alarmlar kapatıldı ve mevcut alarm durumu sıfırlandı");
    } else {
        Serial.println("Tüm alarmlar açıldı");
    }
}

bool AlarmManager::areAlarmsEnabled() const {
    return _areAlarmsEnabled;
}

AlarmType AlarmManager::checkAlarms(float currentTemp, float targetTemp, 
                                   float currentHumid, float targetHumid,
                                   bool motorState, bool isMotorTimeCorrect,
                                   bool sensorsWorking) {
    // Alarmlar devre dışıysa veya eski disable sistemi aktifse - DÜZELTME
    if (!_areAlarmsEnabled || _isAlarmDisabled) {
        // KRİTİK DÜZELTME: Alarmlar kapalıysa mevcut alarm durumunu sıfırla
        if (_currentAlarm != ALARM_NONE || _isAlarmActive) {
            _currentAlarm = ALARM_NONE;
            _isAlarmActive = false;
            digitalWrite(ALARM_PIN, LOW);
        }
        return ALARM_NONE;
    }
    
    // Sensör arızası alarmı (en yüksek öncelik)
    if (!sensorsWorking) {
        _currentAlarm = ALARM_SENSOR;
        _isAlarmActive = true;
        return _currentAlarm;
    }
    
    // Sıcaklık alarmları
    if (currentTemp < (targetTemp - _tempLowThreshold)) {
        _currentAlarm = ALARM_TEMP_LOW;
        _isAlarmActive = true;
        return _currentAlarm;
    }
    
    if (currentTemp > (targetTemp + _tempHighThreshold)) {
        _currentAlarm = ALARM_TEMP_HIGH;
        _isAlarmActive = true;
        return _currentAlarm;
    }
    
    // Nem alarmları
    if (currentHumid < (targetHumid - _humidLowThreshold)) {
        _currentAlarm = ALARM_HUMID_LOW;
        _isAlarmActive = true;
        return _currentAlarm;
    }
    
    if (currentHumid > (targetHumid + _humidHighThreshold)) {
        _currentAlarm = ALARM_HUMID_HIGH;
        _isAlarmActive = true;
        return _currentAlarm;
    }
    
    // Motor alarmı
    if (!isMotorTimeCorrect) {
        _currentAlarm = ALARM_MOTOR;
        _isAlarmActive = true;
        return _currentAlarm;
    }
    
    // Hiçbir alarm yoksa
    if (_isAlarmActive) {
        resetAlarm();
    }
    
    return ALARM_NONE;
}

void AlarmManager::resetAlarm() {
    _currentAlarm = ALARM_NONE;
    _isAlarmActive = false;
    digitalWrite(ALARM_PIN, LOW);
}

AlarmType AlarmManager::getCurrentAlarm() const {
    // KRİTİK DÜZELTME: Alarmlar kapalıysa hiçbir zaman alarm döndürme
    if (!_areAlarmsEnabled) {
        return ALARM_NONE;
    }
    return _currentAlarm;
}

bool AlarmManager::isAlarmActive() const {
    // KRİTİK DÜZELTME: Alarmlar kapalıysa asla aktif alarm yok
    if (!_areAlarmsEnabled) {
        return false;
    }
    return _isAlarmActive;
}

void AlarmManager::update() {
    // KRİTİK DÜZELTME: Alarmlar kapalıysa hiçbir alarm işlemi yapma
    if (!_areAlarmsEnabled) {
        digitalWrite(ALARM_PIN, LOW);
        return;
    }
    
    if (_isAlarmActive && !_isAlarmDisabled) {
        _toggleAlarmIndicator();
    } else {
        digitalWrite(ALARM_PIN, LOW);
    }
}

String AlarmManager::getAlarmMessage() const {
    // KRİTİK DÜZELTME: Alarmlar kapalıysa boş mesaj döndür
    if (!_areAlarmsEnabled) {
        return "";
    }
    
    switch (_currentAlarm) {
        case ALARM_TEMP_LOW:
            return "Dusuk Sicaklik!";
        case ALARM_TEMP_HIGH:
            return "Yuksek Sicaklik!";
        case ALARM_HUMID_LOW:
            return "Dusuk Nem!";
        case ALARM_HUMID_HIGH:
            return "Yuksek Nem!";
        case ALARM_MOTOR:
            return "Motor Hatasi!";
        case ALARM_SENSOR:
            return "Sensor Hatasi!";
        default:
            return "";
    }
}

void AlarmManager::setSoundEnabled(bool enabled) {
    _isSoundEnabled = enabled;
}

bool AlarmManager::isSoundEnabled() const {
    return _isSoundEnabled;
}

void AlarmManager::disableAlarm(bool disabled) {
    _isAlarmDisabled = disabled;
    
    if (disabled) {
        resetAlarm();
    }
}

bool AlarmManager::isAlarmDisabled() const {
    return _isAlarmDisabled;
}

void AlarmManager::_toggleAlarmIndicator() {
    unsigned long currentMillis = millis();
    
    // 500ms aralıklarla LED ve buzzer durumunu değiştir
    if (currentMillis - _lastAlarmToggleTime >= 500) {
        _lastAlarmToggleTime = currentMillis;
        _alarmLedState = !_alarmLedState;
        
        // Alarm ses açıksa buzzer'ı da çalıştır
        if (_isSoundEnabled) {
            digitalWrite(ALARM_PIN, _alarmLedState ? HIGH : LOW);
        }
        // Sadece görsel alarm için (ses kapalıysa)
        else {
            // Burada LED kontrolü yapılabilir (bu örnekte LED için ayrı pin tanımlanmadı)
        }
    }
}