/**
 * @file relays.h
 * @brief SSR röle kontrol modülü
 * @version 1.1
 */

#ifndef RELAYS_H
#define RELAYS_H

#include <Arduino.h>
#include "config.h"

// Forward declaration
class Storage;

class Relays {
public:
    // Yapılandırıcı
    Relays();
    
    // Röleleri başlat
    bool begin();
    
    // Isıtıcı rölesini kontrol et
    void setHeater(bool state);
    
    // Nem rölesini kontrol et
    void setHumidifier(bool state);
    
    // Motor rölesini kontrol et
    void setMotor(bool state);
    
    // Isıtıcı röle durumunu al
    bool getHeaterState() const;
    
    // Nem röle durumunu al
    bool getHumidifierState() const;
    
    // Motor röle durumunu al
    bool getMotorState() const;
    
    // Motor için zamanlama yönetimi
    void updateMotorTiming(unsigned long currentMillis, uint32_t waitTimeMinutes, uint32_t runTimeSeconds);
    
    // Motor rölesinin çalışmasına kalan süreyi al (dakika)
    uint32_t getMotorWaitTimeLeft() const;
    
    // Motor rölesinin çalışma süresinden kalan süreyi al (saniye)
    uint32_t getMotorRunTimeLeft() const;
    
    // Tüm röleleri kapat
    void turnOffAll();
    
    // Röle durumlarını güncelle (ana döngü içinde çağrılmalı)
    void update();
    
    // **EKLEME: Motor zamanlama durumunu storage'dan yükle**
    void loadMotorTimingFromStorage(Storage* storage);
    
    // **EKLEME: Motor zamanlama durumunu storage'a kaydet**
    void saveMotorTimingToStorage(Storage* storage);

private:
    // Röle durumları
    bool _heaterState;
    bool _humidifierState;
    bool _motorState;
    
    // Motor zamanlama değişkenleri
    unsigned long _lastMotorStartTime;
    unsigned long _lastMotorStopTime;
    uint32_t _motorWaitTimeMinutes;
    uint32_t _motorRunTimeSeconds;
    
    // **EKLEME: Motor zamanlama ilk başlatma kontrolü**
    bool _motorTimingInitialized;
    
    // Motor zaman durumları
    enum MotorTimingState {
        WAITING,
        RUNNING
    };
    
    MotorTimingState _motorTimingState;
};

#endif // RELAYS_H