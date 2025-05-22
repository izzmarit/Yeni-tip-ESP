/**
 * @file pid_auto_tune.h
 * @brief PID Otomatik Ayarlama
 * @version 1.0
 */

#ifndef PID_AUTO_TUNE_H
#define PID_AUTO_TUNE_H

#include <Arduino.h>
#include "config.h"

class PIDAutoTune {
public:
    // Yapılandırıcı
    PIDAutoTune();
    
    // Otomatik ayarlamayı başlat
    void start(double setpoint, double *input, bool *output);
    
    // Otomatik ayarlamayı durdur
    void cancel();
    
    // Durumu güncelle
    void update();
    
    // Otomatik ayarlama tamamlandı mı?
    bool isFinished() const;
    
    // PID parametrelerini al
    double getKp() const;
    double getKi() const;
    double getKd() const;
    
    // Son ısıtıcı açık kalma süresini ayarla
    void setLastOnTime(unsigned long time);
    
    // Son ısıtıcı kapalı kalma süresini ayarla
    void setLastOffTime(unsigned long time);
    
    // Otomatik ayarlama durumunun yüzdesini al (0-100)
    int getProgress() const;
    
    // Maksimum güvenli sıcaklık değerini al
    double getMaxTemperature() const;
    
    // Minimum güvenli sıcaklık değerini al
    double getMinTemperature() const;

private:
    double _setpoint;       // Hedef sıcaklık
    double *_input;         // Mevcut sıcaklık
    bool *_output;          // Isıtıcı durumu
    
    // Salınım tespit değişkenleri
    double _peak1;          // İlk tepe noktası
    double _peak2;          // İkinci tepe noktası
    double _lastPeak;       // Son tepe noktası
    
    // Zaman ölçümleri
    unsigned long _peakTimestamp1; // İlk tepe zamanı
    unsigned long _peakTimestamp2; // İkinci tepe zamanı
    unsigned long _startTime;      // Başlangıç zamanı
    
    // Salınım süresi ve genliği
    double _period;         // Salınım periyodu
    double _amplitude;      // Salınım genliği
    
    // Isıtıcı zamanları
    unsigned long _lastOnTime;  // Son ısıtıcı açık kalma süresi
    unsigned long _lastOffTime; // Son ısıtıcı kapalı kalma süresi
    
    // Otomatik ayarlama durumu
    enum AutoTuneState {
        INIT,
        WAITING_FOR_PEAK1,
        WAITING_FOR_PEAK2,
        FINISHED,
        CANCELED
    };
    AutoTuneState _state;
    
    // Otomatik ayarlama parametreleri
    double _Ku;             // Kritik kazanç
    double _Tu;             // Kritik periyot
    
    // PID parametreleri
    double _kp;
    double _ki;
    double _kd;
    
    // Ziegler-Nichols metodu ile PID parametrelerini hesapla
    void _calculateZieglerNichols();
    
    // Geçerli zaman
    unsigned long _currentTime;
    
    // Son giriş değeri
    double _lastInput;
    
    // Progress takibi
    int _progress;
    
    // Güvenlik kontrolleri
    unsigned long _lastSafetyCheckTime; // Son güvenlik kontrolü zamanı
    double _maxTemperature;             // Maksimum güvenli sıcaklık
    double _minTemperature;             // Minimum güvenli sıcaklık
    
    // Tepe noktası tespiti
    bool _isPeak();
};

#endif // PID_AUTO_TUNE_H