/**
 * @file pid.h
 * @brief PID (Proportional-Integral-Derivative) kontrolü
 * @version 1.1
 */

#ifndef PID_H
#define PID_H

#include <Arduino.h>
#include <PID_v1.h>
#include "config.h"
#include "pid_auto_tune.h"

// PID çalışma modları
enum PIDMode {
    PID_MODE_OFF,       // PID kapalı
    PID_MODE_MANUAL,    // Manuel PID
    PID_MODE_AUTO_TUNE  // Otomatik ayarlama
};

class PIDController {
public:
    // Yapılandırıcı
    PIDController();
    
    // Yıkıcı - bellek sızıntısını önlemek için
    ~PIDController();
    
    // PID kontrolünü başlat
    bool begin();
    
    // PID parametrelerini ayarla
    void setTunings(double kp, double ki, double kd);
    
    // PID hedef değerini ayarla
    void setSetpoint(double setpoint);
    
    // Mevcut değeri hesapla ve çıkışı güncelle
    void compute(double input);
    
    // PID çıkış değerini al
    double getOutput() const;
    
    // PID parametrelerini al
    double getKp() const;
    double getKi() const;
    double getKd() const;
    
    // PID hedef değerini al
    double getSetpoint() const;
    
    // PID çıkışının aktif olup olmadığını belirle
    bool isOutputActive() const;
    
    // PID kontrolünün aktif olup olmadığını ayarla
    void setMode(bool active);
    
    // PID hata değerini al
    double getError() const;
    
    // PID modunu ayarla
    void setPIDMode(PIDMode mode);
    
    // Mevcut PID modunu al
    PIDMode getPIDMode() const;
    
    // Manuel PID modunu başlat
    void startManualMode();
    
    // Otomatik-ayarlama modu kontrolü
    void setAutoTuneMode(bool enabled);
    bool isAutoTuneEnabled() const;
    
    // Otomatik ayarlama tamamlandı mı?
    bool isAutoTuneFinished() const;
    
    // Otomatik ayarlama ilerleme yüzdesi
    int getAutoTuneProgress() const;
    
    // Otomatik ayarlamayı başlat
    void startAutoTune();
    
    // PID modunun string halini al
    String getPIDModeString() const;
    
    // Manuel mod aktif mi?
    bool isManualModeActive() const;

private:
    // PID parametreleri
    double _kp;
    double _ki;
    double _kd;
    
    // PID değişkenleri
    double _input;
    double _output;
    double _setpoint;
    
    // PID nesnesi
    PID *_pid;
    
    // PID aktif mi?
    bool _active;
    
    // PID modu
    PIDMode _currentMode;
    
    // PID çıkışının aktif olma eşik değeri
    const double _activationThreshold = 0.3; // Sıcaklık için 0.3°C
    
    // Son hesaplanan hata değeri
    double _lastError;
    
    // Otomatik ayarlama modu
    bool _autoTuneMode;
    
    // Otomatik ayarlama nesnesi
    PIDAutoTune _autoTuner;
    
    // Isıtıcı durumu
    bool _heaterState;
};

#endif // PID_H