/**
 * @file incubation.h
 * @brief Kuluçka tipleri ve kontrolü
 * @version 1.2 - Kuluçka süresi bitiminde otomatik durdurmama düzeltmesi
 */

#ifndef INCUBATION_H
#define INCUBATION_H

#include <Arduino.h>
#include <RTClib.h>
#include "config.h"

// Kuluçka aşamaları
enum IncubationStage {
    STAGE_DEVELOPMENT,  // Gelişim aşaması
    STAGE_HATCHING      // Çıkım aşaması
};

// Kuluçka parametreleri yapısı
struct IncubationParameters {
    float developmentTemp;         // Gelişim aşaması sıcaklığı (°C)
    float hatchingTemp;            // Çıkım aşaması sıcaklığı (°C)
    uint8_t developmentHumidity;   // Gelişim aşaması nemi (%)
    uint8_t hatchingHumidity;      // Çıkım aşaması nemi (%)
    uint8_t developmentDays;       // Gelişim aşaması gün sayısı
    uint8_t hatchingDays;          // Çıkım aşaması gün sayısı
    uint8_t totalDays;             // Toplam kuluçka süresi (gün)
    String name;                   // Kuluçka türü adı
};

class Incubation {
public:
    // Yapılandırıcı
    Incubation();
    
    // Kuluçka modülünü başlat
    bool begin();
    
    // Kuluçka türünü ayarla
    void setIncubationType(uint8_t type);
    
    // Mevcut kuluçka türünü al
    uint8_t getIncubationType() const;
    
    // Kuluçka türü adını al
    String getIncubationTypeName() const;
    
    // Kuluçka başlat
    bool startIncubation(DateTime startTime);
    
    // Kuluçka durdur
    void stopIncubation();
    
    // Kuluçka devam ediyor mu?
    bool isIncubationRunning() const;
    
    // YENİ: Kuluçka tamamlandı mı? (süre doldu ama çalışmaya devam ediyor)
    bool isIncubationCompleted() const;
    
    // Kuluçka parametrelerini al
    IncubationParameters getParameters() const;
    
    // Manuel kuluçka parametrelerini ayarla
    void setManualParameters(float devTemp, float hatchTemp, uint8_t devHumid, 
                             uint8_t hatchHumid, uint8_t devDays, uint8_t hatchDays);
    
    // Hedef sıcaklığı ayarla
    void setTargetTemperature(float temperature);
    
    // Hedef nemi ayarla
    void setTargetHumidity(uint8_t humidity);
    
    // Mevcut kuluçka aşamasını al
    IncubationStage getCurrentStage() const;
    
    // Mevcut kuluçka gününü al (gerçek gün sayısı)
    uint8_t getCurrentDay(DateTime currentTime) const;
    
    // YENİ: Ekranda gösterilecek gün sayısını al (maksimum toplam gün sayısı ile sınırlı)
    uint8_t getDisplayDay(DateTime currentTime) const;
    
    // Kuluçka başlangıç zamanını al
    DateTime getStartTime() const;
    
    // Mevcut hedef sıcaklığı al
    float getTargetTemperature() const;
    
    // Mevcut hedef nemi al
    uint8_t getTargetHumidity() const;
    
    // Mevcut toplam gün sayısını al
    uint8_t getTotalDays() const;
    
    // Kuluçka durumunu güncelle
    void update(DateTime currentTime);
    
    // Durum ve parametre değişikliklerini kaydet
    void saveState();
    
    // Durum ve parametre değişikliklerini yükle
    void loadState();

private:
    // Kuluçka parametreleri
    IncubationParameters _chickenParams;
    IncubationParameters _quailParams;
    IncubationParameters _gooseParams;
    IncubationParameters _manualParams;
    
    // Aktif kuluçka türü
    uint8_t _activeType;
    
    // Kuluçka durumu
    bool _isRunning;
    bool _isCompleted;  // YENİ: Kuluçka süresi tamamlandı mı?
    DateTime _startTime;
    IncubationStage _currentStage;
    
    // Kuluçka parametrelerini başlat
    void _initializeParameters();
    
    // Mevcut kuluçka parametrelerini al
    IncubationParameters _getCurrentParameters() const;
    
    // Belirli bir tip için kuluçka parametrelerini al
    IncubationParameters _getParametersForType(uint8_t type) const;
};

#endif // INCUBATION_H