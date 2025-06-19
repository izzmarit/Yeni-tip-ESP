/**
 * @file watchdog_manager.cpp
 * @brief Gelişmiş Watchdog timer yönetimi uygulaması
 * @version 2.0 - Kritik işlem tracking ve dinamik timeout
 */

#include "watchdog_manager.h"

WatchdogManager::WatchdogManager() {
    _currentState = WD_NORMAL;
    _currentOperation = OP_CUSTOM;
    _operationDescription = "";
    _currentTimeout = WDT_TIMEOUT;
    _operationStartTime = 0;
    _lastFeedTime = 0;
    _stateChangeTime = 0;
    _feedCount = 0;
    _timeoutCount = 0;
    _longestOperationDuration = 0;
    _historyIndex = 0;
    _timeoutWarningCallback = nullptr;
    
    // İşlem geçmişini temizle
    for (int i = 0; i < 10; i++) {
        _operationHistory[i].type = OP_CUSTOM;
        _operationHistory[i].description = "";
        _operationHistory[i].startTime = 0;
        _operationHistory[i].duration = 0;
        _operationHistory[i].completed = false;
    }
}

bool WatchdogManager::begin() {
    Serial.println("Gelişmiş Watchdog Timer başlatılıyor...");
    
    // Watchdog timer'ı başlat
    esp_task_wdt_init(_currentTimeout, WDT_PANIC_MODE);
    esp_task_wdt_add(NULL);
    
    _lastFeedTime = millis();
    _stateChangeTime = millis();
    
    Serial.println("Watchdog Timer başlatıldı - Timeout: " + String(_currentTimeout) + "s");
    return true;
}

void WatchdogManager::feed() {
    esp_task_wdt_reset();
    _lastFeedTime = millis();
    _feedCount++;
    
    // Timeout uyarısını kontrol et
    _checkTimeoutWarning();
    
    // Her 120 saniyede bir durum bilgisi logla
    if (_feedCount % 60 == 0) {
        _logSystemState();
    }
}

void WatchdogManager::beginOperation(OperationType opType, const String& description) {
    // Önceki işlemi tamamla
    if (_currentState != WD_NORMAL) {
        endOperation();
    }
    
    _currentOperation = opType;
    _operationDescription = description;
    _operationStartTime = millis();
    _currentState = WD_LONG_OPERATION;
    _stateChangeTime = millis();
    
    // İşlem için uygun timeout değerini ayarla
    unsigned long newTimeout = _getTimeoutForOperation(opType);
    if (newTimeout != _currentTimeout) {
        _reconfigureWatchdog(newTimeout);
    }
    
    // İşlem geçmişini güncelle
    _updateOperationHistory(opType, description, true);
    
    // İlk beslemeleri yap
    feed();
    
    Serial.println("İşlem başlatıldı: " + description + " (Timeout: " + String(_currentTimeout) + "s)");
}

void WatchdogManager::endOperation() {
    if (_currentState == WD_NORMAL) {
        return;
    }
    
    // İşlem süresini hesapla
    unsigned long operationDuration = millis() - _operationStartTime;
    if (operationDuration > _longestOperationDuration) {
        _longestOperationDuration = operationDuration;
    }
    
    // İşlem geçmişini güncelle
    _updateOperationHistory(_currentOperation, _operationDescription, false);
    
    Serial.println("İşlem tamamlandı: " + _operationDescription + 
                   " (Süre: " + String(operationDuration) + "ms)");
    
    // Normal moda dön
    _currentState = WD_NORMAL;
    _currentOperation = OP_CUSTOM;
    _operationDescription = "";
    _stateChangeTime = millis();
    
    // Normal timeout'a geri dön
    if (_currentTimeout != WDT_TIMEOUT) {
        _reconfigureWatchdog(WDT_TIMEOUT);
    }
    
    feed();
}

void WatchdogManager::enterCriticalSection() {
    _currentState = WD_CRITICAL_SECTION;
    _stateChangeTime = millis();
    
    // Kritik bölüm için kısa timeout
    unsigned long criticalTimeout = WDT_TIMEOUT / 2;
    if (_currentTimeout != criticalTimeout) {
        _reconfigureWatchdog(criticalTimeout);
    }
    
    feed();
    Serial.println("Kritik bölüm başlatıldı");
}

void WatchdogManager::exitCriticalSection() {
    if (_currentState != WD_CRITICAL_SECTION) {
        return;
    }
    
    _currentState = WD_NORMAL;
    _stateChangeTime = millis();
    
    // Normal timeout'a geri dön
    if (_currentTimeout != WDT_TIMEOUT) {
        _reconfigureWatchdog(WDT_TIMEOUT);
    }
    
    feed();
    Serial.println("Kritik bölüm tamamlandı");
}

void WatchdogManager::setEmergencyMode(bool enabled) {
    if (enabled) {
        _currentState = WD_EMERGENCY;
        _stateChangeTime = millis();
        
        // Acil durum için en uzun timeout
        unsigned long emergencyTimeout = WDT_LONG_TIMEOUT * 2;
        if (_currentTimeout != emergencyTimeout) {
            _reconfigureWatchdog(emergencyTimeout);
        }
        
        Serial.println("ACİL DURUM MODU AÇILDI - Timeout: " + String(_currentTimeout) + "s");
    } else {
        _currentState = WD_NORMAL;
        _stateChangeTime = millis();
        
        if (_currentTimeout != WDT_TIMEOUT) {
            _reconfigureWatchdog(WDT_TIMEOUT);
        }
        
        Serial.println("Acil durum modu kapatıldı");
    }
    
    feed();
}

WatchdogState WatchdogManager::getCurrentState() const {
    return _currentState;
}

unsigned long WatchdogManager::getRemainingTime() const {
    unsigned long elapsedTime = (millis() - _lastFeedTime) / 1000;
    if (elapsedTime >= _currentTimeout) {
        return 0;
    }
    return _currentTimeout - elapsedTime;
}

String WatchdogManager::getOperationHistory() const {
    String history = "Son İşlemler:\n";
    
    for (int i = 0; i < 10; i++) {
        int index = (_historyIndex - i - 1 + 10) % 10;
        const OperationRecord& record = _operationHistory[index];
        
        if (record.startTime == 0) continue;
        
        String opName;
        switch (record.type) {
            case OP_WIFI_CONNECT: opName = "WiFi"; break;
            case OP_STORAGE_WRITE: opName = "Storage"; break;
            case OP_SENSOR_READ: opName = "Sensor"; break;
            case OP_DISPLAY_UPDATE: opName = "Display"; break;
            case OP_MENU_NAVIGATION: opName = "Menu"; break;
            case OP_PID_AUTOTUNE: opName = "PID"; break;
            case OP_SYSTEM_INIT: opName = "Init"; break;
            default: opName = "Custom"; break;
        }
        
        history += String(i + 1) + ". " + opName + ": " + record.description + 
                   " (" + String(record.duration) + "ms) " + 
                   (record.completed ? "✓" : "✗") + "\n";
    }
    
    return history;
}

void WatchdogManager::getStatistics(unsigned long& feedCount, unsigned long& timeoutCount, unsigned long& longestOperation) {
    feedCount = _feedCount;
    timeoutCount = _timeoutCount;
    longestOperation = _longestOperationDuration;
}

void WatchdogManager::setCustomTimeout(unsigned long timeoutSeconds) {
    _reconfigureWatchdog(timeoutSeconds);
    Serial.println("Özel timeout ayarlandı: " + String(timeoutSeconds) + "s");
}

void WatchdogManager::setTimeoutWarningCallback(void (*callback)(unsigned long remainingTime)) {
    _timeoutWarningCallback = callback;
}

unsigned long WatchdogManager::_getTimeoutForOperation(OperationType opType) const {
    switch (opType) {
        case OP_WIFI_CONNECT:
            return WDT_LONG_TIMEOUT;  // 30 saniye
        case OP_STORAGE_WRITE:
            return WDT_TIMEOUT * 2;   // 20 saniye
        case OP_SENSOR_READ:
            return WDT_TIMEOUT;       // 10 saniye
        case OP_DISPLAY_UPDATE:
            return WDT_TIMEOUT;       // 10 saniye
        case OP_MENU_NAVIGATION:
            return WDT_TIMEOUT;       // 10 saniye
        case OP_PID_AUTOTUNE:
            return WDT_LONG_TIMEOUT * 2; // 60 saniye
        case OP_SYSTEM_INIT:
            return WDT_LONG_TIMEOUT;  // 30 saniye
        default:
            return WDT_TIMEOUT;       // 10 saniye
    }
}

void WatchdogManager::_reconfigureWatchdog(unsigned long timeoutSeconds) {
    // Mevcut watchdog'u durdur
    esp_task_wdt_deinit();
    
    // Yeni timeout ile başlat
    esp_task_wdt_init(timeoutSeconds, WDT_PANIC_MODE);
    esp_task_wdt_add(NULL);
    
    _currentTimeout = timeoutSeconds;
}

void WatchdogManager::_updateOperationHistory(OperationType type, const String& description, bool isStart) {
    if (isStart) {
        // Yeni işlem başlatıldı
        _operationHistory[_historyIndex].type = type;
        _operationHistory[_historyIndex].description = description;
        _operationHistory[_historyIndex].startTime = millis();
        _operationHistory[_historyIndex].duration = 0;
        _operationHistory[_historyIndex].completed = false;
    } else {
        // Mevcut işlem tamamlandı
        _operationHistory[_historyIndex].duration = millis() - _operationHistory[_historyIndex].startTime;
        _operationHistory[_historyIndex].completed = true;
        
        // Sonraki index'e geç
        _historyIndex = (_historyIndex + 1) % 10;
    }
}

void WatchdogManager::_checkTimeoutWarning() {
    unsigned long remainingTime = getRemainingTime();
    
    // Kalan süre 3 saniyeden azsa uyar
    if (remainingTime <= 3 && _timeoutWarningCallback != nullptr) {
        _timeoutWarningCallback(remainingTime);
    }
    
    // Kalan süre 1 saniyeden azsa logla
    if (remainingTime <= 1) {
        Serial.println("UYARI: Watchdog timeout yaklaşıyor! Kalan: " + String(remainingTime) + "s");
        _timeoutCount++;
    }
}

void WatchdogManager::_logSystemState() const {
    String stateStr;
    switch (_currentState) {
        case WD_NORMAL: stateStr = "Normal"; break;
        case WD_LONG_OPERATION: stateStr = "Uzun İşlem"; break;
        case WD_CRITICAL_SECTION: stateStr = "Kritik Bölüm"; break;
        case WD_EMERGENCY: stateStr = "Acil Durum"; break;
    }
    
    Serial.println("Watchdog Durum: " + stateStr + 
                   " | Timeout: " + String(_currentTimeout) + "s" +
                   " | Besleme: " + String(_feedCount) +
                   " | Free Heap: " + String(ESP.getFreeHeap()));
}