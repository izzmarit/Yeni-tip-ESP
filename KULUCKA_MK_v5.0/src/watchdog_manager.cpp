/**
 * @file watchdog_manager.cpp
 * @brief Watchdog timer yönetimi uygulaması
 * @version 1.0
 */

#include "watchdog_manager.h"

WatchdogManager::WatchdogManager() {
    _isEnabled = false;
    _inLongOperation = false;
    _lastLogTime = 0;
}

bool WatchdogManager::begin() {
    // Watchdog timer'ı başlat
    Serial.println("Watchdog Timer baslatiliyor...");
    esp_task_wdt_init(WDT_TIMEOUT, WDT_PANIC_MODE);
    esp_task_wdt_add(NULL);  // Mevcut görevi (NULL = ana görev) watchdog'a ekle
    _isEnabled = true;
    _lastLogTime = millis();
    Serial.println("Watchdog Timer baslatildi.");
    return true;
}

void WatchdogManager::feed() {
    if (_isEnabled) {
        esp_task_wdt_reset();
        
        // Her 60 saniyede bir log mesajı
        unsigned long currentMillis = millis();
        if (currentMillis - _lastLogTime > 60000) {
            Serial.println("Watchdog: Normal besleme");
            _lastLogTime = currentMillis;
        }
    }
}

void WatchdogManager::beginLongOperation() {
    if (_isEnabled && !_inLongOperation) {
        // Watchdog süresini uzat
        esp_task_wdt_deinit(); // Mevcut watchdog'u devre dışı bırak
        esp_task_wdt_init(WDT_LONG_TIMEOUT, WDT_PANIC_MODE); // Uzun timeout ile yeniden başlat
        esp_task_wdt_add(NULL);
        _inLongOperation = true;
        Serial.println("Watchdog: Uzun islem modu baslatildi (" + String(WDT_LONG_TIMEOUT) + "s)");
        feed(); // İlk besleme
    }
}

void WatchdogManager::endLongOperation() {
    if (_isEnabled && _inLongOperation) {
        // Normal süreye geri dön
        esp_task_wdt_deinit(); // Mevcut watchdog'u devre dışı bırak
        esp_task_wdt_init(WDT_TIMEOUT, WDT_PANIC_MODE); // Normal timeout ile yeniden başlat
        esp_task_wdt_add(NULL);
        _inLongOperation = false;
        Serial.println("Watchdog: Normal moda donuldu (" + String(WDT_TIMEOUT) + "s)");
        feed(); // İlk besleme
    }
}

unsigned long WatchdogManager::getRemainingTime() const {
    // ESP32'nin watchdog için kalan süreyi hesapla (yaklaşık değer)
    unsigned long timeout = _inLongOperation ? WDT_LONG_TIMEOUT : WDT_TIMEOUT;
    unsigned long currentMillis = millis();
    unsigned long elapsedTime = (currentMillis - _lastLogTime) / 1000; // saniye cinsinden
    
    if (elapsedTime >= timeout) {
        return 0;
    }
    
    return timeout - elapsedTime;
}

bool WatchdogManager::isInLongOperation() const {
    return _inLongOperation;
}