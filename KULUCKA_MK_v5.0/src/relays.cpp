/**
 * @file relays.cpp
 * @brief SSR röle kontrol modülü uygulaması
 * @version 1.1
 */

#include "relays.h"
#include "storage.h"

Relays::Relays() {
    _heaterState = false;
    _humidifierState = false;
    _motorState = false;
    _lastMotorStartTime = 0;
    _lastMotorStopTime = 0;
    _motorWaitTimeMinutes = DEFAULT_MOTOR_WAIT_TIME;
    _motorRunTimeSeconds = DEFAULT_MOTOR_RUN_TIME;
    _motorTimingState = WAITING;
    _motorTimingInitialized = false;  // **EKLEME: İlk başlatma kontrolü**
}

bool Relays::begin() {
    // Röle pinlerini çıkış olarak ayarla
    pinMode(RELAY_HEAT, OUTPUT);
    pinMode(RELAY_HUMID, OUTPUT);
    pinMode(RELAY_MOTOR, OUTPUT);
    
    // Başlangıçta tüm röleleri kapat
    turnOffAll();
    
    return true;
}

void Relays::setHeater(bool state) {
    _heaterState = state;
    digitalWrite(RELAY_HEAT, state ? HIGH : LOW);
}

void Relays::setHumidifier(bool state) {
    _humidifierState = state;
    digitalWrite(RELAY_HUMID, state ? HIGH : LOW);
}

void Relays::setMotor(bool state) {
    _motorState = state;
    digitalWrite(RELAY_MOTOR, state ? HIGH : LOW);
}

bool Relays::getHeaterState() const {
    return _heaterState;
}

bool Relays::getHumidifierState() const {
    return _humidifierState;
}

bool Relays::getMotorState() const {
    return _motorState;
}

void Relays::updateMotorTiming(unsigned long currentMillis, uint32_t waitTimeMinutes, uint32_t runTimeSeconds) {
    // **DÜZELTME: İlk başlatma durumunu kontrol et**
    if (!_motorTimingInitialized) {
        _lastMotorStopTime = currentMillis;
        _motorTimingState = WAITING;
        _motorTimingInitialized = true;
    }
    
    // Motor bekleme ve çalışma zamanlarını güncelle - parametre değişikliği varsa
    if (_motorWaitTimeMinutes != waitTimeMinutes || _motorRunTimeSeconds != runTimeSeconds) {
        _motorWaitTimeMinutes = waitTimeMinutes;
        _motorRunTimeSeconds = runTimeSeconds;
        
        // Eğer sürelerde değişiklik yapıldıysa, kalan zamanı yeniden hesapla
        if (_motorTimingState == WAITING) {
            // Bekleme durumunda ise, geçen süreyi hesapla ve yeni bekleme süresi ile orantılı ayarla
            unsigned long elapsedMillis = currentMillis - _lastMotorStopTime;
            unsigned long oldTotalWaitMillis = _motorWaitTimeMinutes * 60000UL;
            
            // Eğer eski toplam bekleme süresi 0 değilse ve geçen süre toplam süreden küçükse
            if (oldTotalWaitMillis > 0 && elapsedMillis < oldTotalWaitMillis) {
                // Geçen süre yüzdesini hesapla
                float elapsedPercentage = (float)elapsedMillis / oldTotalWaitMillis;
                
                // Yeni toplam bekleme süresini hesapla
                unsigned long newTotalWaitMillis = waitTimeMinutes * 60000UL;
                
                // Yeni bir başlangıç zamanı ayarla
                _lastMotorStopTime = currentMillis - (unsigned long)(newTotalWaitMillis * elapsedPercentage);
            }
        } else if (_motorTimingState == RUNNING) {
            // Çalışma durumunda ise, geçen süreyi hesapla ve yeni çalışma süresi ile orantılı ayarla
            unsigned long elapsedMillis = currentMillis - _lastMotorStartTime;
            unsigned long oldTotalRunMillis = _motorRunTimeSeconds * 1000UL;
            
            // Eğer eski toplam çalışma süresi 0 değilse ve geçen süre toplam süreden küçükse
            if (oldTotalRunMillis > 0 && elapsedMillis < oldTotalRunMillis) {
                // Geçen süre yüzdesini hesapla
                float elapsedPercentage = (float)elapsedMillis / oldTotalRunMillis;
                
                // Yeni toplam çalışma süresini hesapla
                unsigned long newTotalRunMillis = runTimeSeconds * 1000UL;
                
                // Yeni bir başlangıç zamanı ayarla
                _lastMotorStartTime = currentMillis - (unsigned long)(newTotalRunMillis * elapsedPercentage);
            }
        }
    }
    
    // Watchdog besleme - motor zamanlaması hesaplanırken
    esp_task_wdt_reset();
    
    // Motor zaman durumuna göre işlem yap
    switch (_motorTimingState) {
        case WAITING:
            if (currentMillis - _lastMotorStopTime >= (_motorWaitTimeMinutes * 60000UL)) {
                setMotor(true);
                _lastMotorStartTime = currentMillis;
                _motorTimingState = RUNNING;
                
                // DÜZELTME: extern yerine member değişken kullan
                if (_storage != nullptr) {
                    saveMotorTimingToStorage(_storage);
                }
            }
            break;
            
        case RUNNING:
            if (currentMillis - _lastMotorStartTime >= (_motorRunTimeSeconds * 1000UL)) {
                setMotor(false);
                _lastMotorStopTime = currentMillis;
                _motorTimingState = WAITING;
                
                // DÜZELTME: extern yerine member değişken kullan
                if (_storage != nullptr) {
                    saveMotorTimingToStorage(_storage);
                }
            }
            break;
    }
    
    // Watchdog besleme - motor kontrolü sonrası
    esp_task_wdt_reset();
}

uint32_t Relays::getMotorWaitTimeLeft() const {
    if (_motorTimingState != WAITING) {
        return 0;
    }
    
    unsigned long currentMillis = millis();
    unsigned long elapsedMillis = currentMillis - _lastMotorStopTime;
    unsigned long totalWaitMillis = _motorWaitTimeMinutes * 60000UL;
    
    if (elapsedMillis >= totalWaitMillis) {
        return 0;
    }
    
    // Kalan zamanı milisaniyeden dakikaya çevir (yukarı yuvarlayarak)
    return (totalWaitMillis - elapsedMillis + 59999) / 60000UL;
}

uint32_t Relays::getMotorRunTimeLeft() const {
    if (_motorTimingState == RUNNING) {
        // Motor çalışırken geri sayım yap
        unsigned long currentMillis = millis();
        unsigned long elapsedMillis = currentMillis - _lastMotorStartTime;
        unsigned long totalRunMillis = _motorRunTimeSeconds * 1000UL;
        
        if (elapsedMillis >= totalRunMillis) {
            return 0;
        }
        
        // Kalan zamanı milisaniyeden saniyeye çevir
        return (totalRunMillis - elapsedMillis) / 1000UL;
    } else {
        // Motor pasifken ayarlanan çalışma süresini göster
        return _motorRunTimeSeconds;
    }
}

void Relays::turnOffAll() {
    setHeater(false);
    setHumidifier(false);
    setMotor(false);
}

void Relays::update() {
    // Motor zamanlama güncellemesi
    updateMotorTiming(millis(), _motorWaitTimeMinutes, _motorRunTimeSeconds);
}

void Relays::loadMotorTimingFromStorage(Storage* storage) {
    if (storage == nullptr) return;
    
    uint32_t savedLastActionTime = storage->getMotorLastActionTime();
    uint8_t savedState = storage->getMotorTimingState();
    uint32_t savedElapsedTime = storage->getMotorElapsedTime();
    
    unsigned long currentMillis = millis();
    
    if (savedState == 1) { // RUNNING durumunda kapatılmış
        _motorState = false;
        _lastMotorStopTime = currentMillis;
        _motorTimingState = WAITING;
        _motorTimingInitialized = true;
        
        Serial.println("Motor: Sistem yeniden başlatıldı, motor güvenlik için kapatıldı");
    } else if (savedState == 0 && savedElapsedTime > 0) { // WAITING durumunda
        _lastMotorStopTime = currentMillis - savedElapsedTime;
        _motorTimingState = WAITING;
        _motorTimingInitialized = true;
        
        uint32_t remainingWait = (_motorWaitTimeMinutes * 60000UL) - savedElapsedTime;
        Serial.println("Motor: Bekleme durumu restore edildi. Kalan: " + 
                      String(remainingWait / 60000) + " dakika");
    } else {
        _lastMotorStopTime = currentMillis;
        _motorTimingState = WAITING;
        _motorTimingInitialized = true;
    }
}

void Relays::saveMotorTimingToStorage(Storage* storage) {
    if (storage == nullptr) return;
    
    unsigned long currentMillis = millis();
    uint32_t elapsedTime = 0;
    
    if (_motorTimingState == RUNNING) {
        elapsedTime = currentMillis - _lastMotorStartTime;
        storage->setMotorTimingState(1); // RUNNING
    } else {
        elapsedTime = currentMillis - _lastMotorStopTime;
        storage->setMotorTimingState(0); // WAITING
    }
    
    storage->setMotorLastActionTime(currentMillis);
    storage->setMotorElapsedTime(elapsedTime);
}

void Relays::performMotorTest(uint32_t durationSeconds) {
    // Motor test modunda normal zamanlama işlemlerini bypass et
    Serial.println("Relays: Motor test başlatılıyor - Süre: " + String(durationSeconds) + " saniye");
    
    // Motoru hemen çalıştır
    setMotor(true);
    
    // Test süresi bilgisini kaydet (opsiyonel - log amaçlı)
    unsigned long testStartTime = millis();
    
    // Not: Gerçek bekleme ve motor durdurma işlemi main.cpp'deki 
    // performMotorTest() fonksiyonunda yapılacak
}