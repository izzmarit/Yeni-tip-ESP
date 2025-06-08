/**
 * @file watchdog_manager.h
 * @brief Watchdog timer yönetimi
 * @version 1.0
 */

#ifndef WATCHDOG_MANAGER_H
#define WATCHDOG_MANAGER_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"

class WatchdogManager {
public:
    // Yapılandırıcı
    WatchdogManager();
    
    // Watchdog timer'ı başlat
    bool begin();
    
    // Watchdog timer'ı besle (normal)
    void feed();
    
    // Uzun işlem için watchdog süresini artır
    void beginLongOperation();
    
    // Uzun işlem modunu sonlandır
    void endLongOperation();
    
    // Watchdog resetine kalan süreyi hesapla (saniye cinsinden)
    unsigned long getRemainingTime() const;
    
    // Uzun işlem modunda mı?
    bool isInLongOperation() const;
    
private:
    bool _isEnabled;
    bool _inLongOperation;
    unsigned long _lastLogTime;
};

#endif // WATCHDOG_MANAGER_H