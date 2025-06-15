/**
 * @file watchdog_manager.h
 * @brief Gelişmiş Watchdog timer yönetimi - Kritik işlem tracking ve dinamik timeout
 * @version 2.0 - Tamamen yeniden tasarlandı
 */

#ifndef WATCHDOG_MANAGER_H
#define WATCHDOG_MANAGER_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"

// Watchdog durumları
enum WatchdogState {
    WD_NORMAL,              // Normal çalışma modu
    WD_LONG_OPERATION,      // Uzun işlem modu
    WD_CRITICAL_SECTION,    // Kritik bölüm modu
    WD_EMERGENCY           // Acil durum modu
};

// İşlem tipleri
enum OperationType {
    OP_WIFI_CONNECT,       // WiFi bağlantı işlemi
    OP_STORAGE_WRITE,      // Storage yazma işlemi
    OP_STORAGE_READ,       // Storage okuma işlemi - EKLENDİ
    OP_SENSOR_READ,        // Sensör okuma işlemi
    OP_DISPLAY_UPDATE,     // Ekran güncelleme işlemi
    OP_MENU_NAVIGATION,    // Menü navigasyonu
    OP_PID_AUTOTUNE,       // PID otomatik ayarlama
    OP_SYSTEM_INIT,        // Sistem başlatma
    OP_CUSTOM              // Özel işlem
};

class WatchdogManager {
public:
    // Yapılandırıcı
    WatchdogManager();
    
    // Watchdog timer'ı başlat
    bool begin();
    
    // Normal watchdog beslemesi
    void feed();
    
    // İşlem başlangıcını kaydet ve uygun timeout ayarla
    void beginOperation(OperationType opType, const String& description = "");
    
    // İşlem bitişini kaydet ve normal moda dön
    void endOperation();
    
    // Kritik bölüm başlangıcı (kısa timeout)
    void enterCriticalSection();
    
    // Kritik bölüm bitişi
    void exitCriticalSection();
    
    // Acil durum modu (en uzun timeout)
    void setEmergencyMode(bool enabled);
    
    // Mevcut durumu al
    WatchdogState getCurrentState() const;
    
    // Kalan süreyi hesapla
    unsigned long getRemainingTime() const;
    
    // İşlem geçmişini al (debug için)
    String getOperationHistory() const;
    
    // Watchdog istatistiklerini al
    void getStatistics(unsigned long& feedCount, unsigned long& timeoutCount, unsigned long& longestOperation);
    
    // Manuel timeout ayarla (test amaçlı)
    void setCustomTimeout(unsigned long timeoutSeconds);
    
    // Timeout uyarısı callback ayarla
    void setTimeoutWarningCallback(void (*callback)(unsigned long remainingTime));

private:
    WatchdogState _currentState;
    OperationType _currentOperation;
    String _operationDescription;
    
    // Zamanlama değişkenleri
    unsigned long _currentTimeout;
    unsigned long _operationStartTime;
    unsigned long _lastFeedTime;
    unsigned long _stateChangeTime;
    
    // İstatistik değişkenleri
    unsigned long _feedCount;
    unsigned long _timeoutCount;
    unsigned long _longestOperationDuration;
    
    // İşlem geçmişi (son 10 işlem)
    struct OperationRecord {
        OperationType type;
        String description;
        unsigned long startTime;
        unsigned long duration;
        bool completed;
    };
    OperationRecord _operationHistory[10];
    int _historyIndex;
    
    // Callback fonksiyonu
    void (*_timeoutWarningCallback)(unsigned long remainingTime);
    
    // Timeout değerlerini al
    unsigned long _getTimeoutForOperation(OperationType opType) const;
    
    // Watchdog'u yeniden yapılandır
    void _reconfigureWatchdog(unsigned long timeoutSeconds);
    
    // İşlem geçmişini güncelle
    void _updateOperationHistory(OperationType type, const String& description, bool isStart);
    
    // Timeout kontrolü
    void _checkTimeoutWarning();
    
    // Sistem durumunu logla
    void _logSystemState() const;
};

#endif // WATCHDOG_MANAGER_H