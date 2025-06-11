/**
 * @file alarm.h
 * @brief Alarm yönetimi
 * @version 1.2 - Alarm kapatma sorunu düzeltildi
 */

#ifndef ALARM_H
#define ALARM_H

#include <Arduino.h>
#include "config.h"

// Alarm tipleri
enum AlarmType {
    ALARM_NONE,
    ALARM_TEMP_LOW,
    ALARM_TEMP_HIGH,
    ALARM_HUMID_LOW,
    ALARM_HUMID_HIGH,
    ALARM_MOTOR,
    ALARM_SENSOR
};

class AlarmManager {
public:
    // Yapılandırıcı
    AlarmManager();
    
    // Alarm yönetimini başlat
    bool begin();
    
    // Alarm eşik değerlerini ayarla
    void setTempLowThreshold(float value);
    void setTempHighThreshold(float value);
    void setHumidLowThreshold(float value);
    void setHumidHighThreshold(float value);
    
    // Alarm eşik değerlerini al
    float getTempLowThreshold() const;
    float getTempHighThreshold() const;
    float getHumidLowThreshold() const;
    float getHumidHighThreshold() const;
    
    // Alarmı kontrol et
    AlarmType checkAlarms(float currentTemp, float targetTemp, 
                         float currentHumid, float targetHumid,
                         bool motorState, bool isMotorTimeCorrect,
                         bool sensorsWorking);
    
    // Alarm durumunu sıfırla
    void resetAlarm();
    
    // Mevcut alarm tipini al
    AlarmType getCurrentAlarm() const;
    
    // Alarm aktif mi? - YENİ EKLENDİ
    bool isAlarmActive() const;
    
    // Alarm durumunu güncelle
    void update();
    
    // Alarm mesajını al
    String getAlarmMessage() const;
    
    // Alarm ses açık/kapalı
    void setSoundEnabled(bool enabled);
    bool isSoundEnabled() const;
    
    // Tüm alarmları etkinleştir/devre dışı bırak
    void setAlarmsEnabled(bool enabled);
    bool areAlarmsEnabled() const;
    
    // Alarmı devre dışı bırak (eski versiyon uyumluluğu için)
    void disableAlarm(bool disabled);
    bool isAlarmDisabled() const;

private:
    // Alarm eşik değerleri
    float _tempLowThreshold;
    float _tempHighThreshold;
    float _humidLowThreshold;
    float _humidHighThreshold;
    
    // Alarm durumu
    AlarmType _currentAlarm;
    bool _isAlarmActive;
    bool _isSoundEnabled;
    bool _isAlarmDisabled;
    bool _areAlarmsEnabled;  // Tüm alarmlar etkin mi?
    
    // Alarm zamanlaması
    unsigned long _lastAlarmToggleTime;
    bool _alarmLedState;
    
    // Alarm LED ve buzzer kontrolü
    void _toggleAlarmIndicator();
};

#endif // ALARM_H