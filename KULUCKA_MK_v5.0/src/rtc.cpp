/**
 * @file rtc.cpp
 * @brief DS3231 RTC (Real Time Clock) yönetimi uygulaması
 * @version 1.0
 */

#include "rtc.h"

RTCModule::RTCModule() {
    _isRtcRunning = false;
}

bool RTCModule::begin() {
    // RTC başlatma işlemi
    if (!_rtc.begin()) {
        return false;
    }
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    _isRtcRunning = true;
    
    // RTC pilinin durumunu kontrol et
    if (_rtc.lostPower()) {
        // RTC pili bitmiş, varsayılan tarih ve saati ayarla
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    return true;
}

DateTime RTCModule::getCurrentDateTime() {
    if (_isRtcRunning) {
        return _rtc.now();
    } else {
        // RTC çalışmıyorsa varsayılan bir DateTime nesnesi döndür
        return DateTime(2025, 1, 1, 0, 0, 0);
    }
}

String RTCModule::getTimeString() {
    DateTime now = getCurrentDateTime();
    
    char timeStr[6]; // "HH:MM\0"
    sprintf(timeStr, "%02d:%02d", now.hour(), now.minute());
    
    return String(timeStr);
}

String RTCModule::getDateString() {
    DateTime now = getCurrentDateTime();
    
    char dateStr[11]; // "DD.MM.YYYY\0"
    sprintf(dateStr, "%02d.%02d.%04d", now.day(), now.month(), now.year());
    
    return String(dateStr);
}

bool RTCModule::setDateTime(uint8_t hour, uint8_t minute, uint8_t day, uint8_t month, uint16_t year) {
    if (!_isRtcRunning) {
        return false;
    }
    
    // Değerlerin geçerli aralıkta olup olmadığını kontrol et
    if (hour > 23 || minute > 59 || day < 1 || day > 31 || month < 1 || month > 12 || year < 2000 || year > 2100) {
        return false;
    }
    
    // RTC saatini ayarla (saniyeyi 0 olarak ayarla)
    _rtc.adjust(DateTime(year, month, day, hour, minute, 0));
    
    // Watchdog besleme - RTC ayarlama işlemi sonrası
    esp_task_wdt_reset();
    
    return true;
}

uint32_t RTCModule::getElapsedMinutes(DateTime startTime) {
    DateTime now = getCurrentDateTime();
    
    // İki zaman arasındaki farkı saniye cinsinden hesapla
    int32_t diff = now.unixtime() - startTime.unixtime();
    
    // Saniyeden dakikaya dönüştür
    return diff / 60;
}

uint32_t RTCModule::getRemainingMinutes(DateTime targetTime) {
    DateTime now = getCurrentDateTime();
    
    // İki zaman arasındaki farkı saniye cinsinden hesapla
    int32_t diff = targetTime.unixtime() - now.unixtime();
    
    // Negatif değer kontrolü
    if (diff < 0) {
        return 0;
    }
    
    // Saniyeden dakikaya dönüştür
    return diff / 60;
}

uint8_t RTCModule::getSeconds() {
    if (_isRtcRunning) {
        return getCurrentDateTime().second();
    } else {
        return 0;
    }
}

uint32_t RTCModule::getMinutesBetween(DateTime startTime, DateTime endTime) {
    // İki zaman arasındaki farkı saniye cinsinden hesapla
    int32_t diff = endTime.unixtime() - startTime.unixtime();
    
    // Negatif değer kontrolü
    if (diff < 0) {
        return 0;
    }
    
    // Saniyeden dakikaya dönüştür
    return diff / 60;
}