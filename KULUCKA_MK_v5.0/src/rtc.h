/**
 * @file rtc.h
 * @brief DS3231 RTC (Real Time Clock) yönetimi
 * @version 1.0
 */

#ifndef RTC_H
#define RTC_H

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include "config.h"

class RTCModule {
public:
    // Yapılandırıcı
    RTCModule();
    
    // RTC modülünü başlat
    bool begin();
    
    // Mevcut tarih ve saat bilgisini al
    DateTime getCurrentDateTime();
    
    // Saat bilgisini formatla (HH:MM formatında)
    String getTimeString();
    
    // Tarih bilgisini formatla (DD.MM.YYYY formatında)
    String getDateString();
    
    // RTC saatini ayarla
    bool setDateTime(uint8_t hour, uint8_t minute, uint8_t day, uint8_t month, uint16_t year);
    
    // Belirli bir zamana geçen süreyi hesapla (dakika olarak)
    uint32_t getElapsedMinutes(DateTime startTime);
    
    // Belirli bir zamana kalan süreyi hesapla (dakika olarak)
    uint32_t getRemainingMinutes(DateTime targetTime);
    
    // Saniye bilgisini al
    uint8_t getSeconds();
    
    // İki zaman arasında geçen toplam dakikayı hesapla
    uint32_t getMinutesBetween(DateTime startTime, DateTime endTime);
    
    // RTC çalışıyor mu?
    bool isRTCWorking() const;
    
    // RTC hata sayısını al
    int getRTCErrorCount() const;

private:
    RTC_DS3231 _rtc;
    bool _isRtcRunning;
    unsigned long _lastSyncTime;
    int _rtcErrorCount;
    
    // RTC'nin durumunu kontrol et
    void _checkRTC();
    
    // RTC'yi yeniden başlat
    void _restartRTC();
};

#endif // RTC_H