/**
 * @file rtc.cpp
 * @brief DS3231 RTC (Real Time Clock) yönetimi uygulaması
 * @version 1.0
 */

#include "rtc.h"

RTCModule::RTCModule() {
    _isRtcRunning = false;
    _lastSyncTime = 0;
    _rtcErrorCount = 0;
}

bool RTCModule::begin() {
    // RTC başlatma denemesi
    for (int attempt = 0; attempt < 3; attempt++) {
        if (_rtc.begin()) {
            _isRtcRunning = true;
            _rtcErrorCount = 0;
            
            // RTC pilinin durumunu kontrol et
            if (_rtc.lostPower()) {
                // RTC pili bitmiş, varsayılan tarih ve saati ayarla
                _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
                Serial.println("RTC pili bitmiş, varsayılan tarih ve saat ayarlandı");
            }
            
            Serial.println("RTC başlatıldı");
            return true;
        }
        
        Serial.print("RTC başlatma hatası, tekrar deneniyor (");
        Serial.print(attempt + 1);
        Serial.println("/3)");
        
        // I2C hatası durumunda bus'ı sıfırla
        Wire.end();
        delay(100);
        Wire.begin(I2C_SDA, I2C_SCL);
        delay(50);
    }
    
    Serial.println("RTC başlatılamadı!");
    return false;
}

DateTime RTCModule::getCurrentDateTime() {
    if (!_isRtcRunning) {
        Serial.println("RTC çalışmıyor, varsayılan tarih döndürülüyor");
        return DateTime(2025, 1, 1, 0, 0, 0);
    }
    
    // 5 dakikada bir RTC'yi kontrol et ve gerekirse yeniden başlat
    unsigned long currentMillis = millis();
    if (currentMillis - _lastSyncTime > 300000) { // 5 dakika
        _lastSyncTime = currentMillis;
        _checkRTC();
    }
    
    try {
        DateTime now = _rtc.now();
        
        // Geçerli tarih kontrolü
        if (now.year() < 2023 || now.year() > 2100) {
            Serial.println("RTC geçersiz yıl: " + String(now.year()));
            _rtcErrorCount++;
            if (_rtcErrorCount > 3) {
                _restartRTC();
            }
            return DateTime(2025, 1, 1, 0, 0, 0);
        }
        
        return now;
    } catch (...) {
        Serial.println("RTC veri okuma hatası!");
        _rtcErrorCount++;
        
        if (_rtcErrorCount > 3) {
            _restartRTC();
        }
        
        return DateTime(2025, 1, 1, 0, 0, 0);
    }
}

void RTCModule::_checkRTC() {
    // RTC hala çalışıyor mu kontrol et
    if (!_rtc.begin()) {
        Serial.println("RTC bağlantı hatası tespit edildi, yeniden başlatılıyor...");
        _restartRTC();
    } else {
        _rtcErrorCount = 0; // Başarılı kontrol, hata sayacını sıfırla
    }
}

void RTCModule::_restartRTC() {
    Serial.println("RTC yeniden başlatılıyor...");
    
    // I2C bus'ı yeniden başlat
    Wire.end();
    delay(100);
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(50);
    
    // RTC'yi yeniden başlat
    if (_rtc.begin()) {
        _isRtcRunning = true;
        _rtcErrorCount = 0;
        Serial.println("RTC başarıyla yeniden başlatıldı");
    } else {
        _isRtcRunning = false;
        Serial.println("RTC yeniden başlatılamadı!");
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
        Serial.println("RTC çalışmıyor, tarih/saat ayarlanamadı");
        return false;
    }
    
    // Değerlerin geçerli aralıkta olup olmadığını kontrol et
    if (hour > 23 || minute > 59 || day < 1 || day > 31 || month < 1 || month > 12 || year < 2023 || year > 2100) {
        Serial.println("Geçersiz tarih/saat değerleri!");
        return false;
    }
    
    try {
        // RTC saatini ayarla (saniyeyi 0 olarak ayarla)
        _rtc.adjust(DateTime(year, month, day, hour, minute, 0));
        
        Serial.println("RTC tarih/saat ayarlandı: " + 
                      String(day) + "." + String(month) + "." + String(year) + " " + 
                      String(hour) + ":" + String(minute));
        
        // Watchdog besleme - RTC ayarlama işlemi sonrası
        esp_task_wdt_reset();
        
        return true;
    } catch (...) {
        Serial.println("RTC tarih/saat ayarlama hatası!");
        _rtcErrorCount++;
        
        if (_rtcErrorCount > 3) {
            _restartRTC();
        }
        
        return false;
    }
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

bool RTCModule::isRTCWorking() const {
    return _isRtcRunning;
}

int RTCModule::getRTCErrorCount() const {
    return _rtcErrorCount;
}