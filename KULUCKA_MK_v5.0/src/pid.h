/**
 * @file pid.h
 * @brief PID (Proportional-Integral-Derivative) kontrolü
 * @version 1.0
 */

#ifndef PID_H
#define PID_H

#include <Arduino.h>
#include <PID_v1.h>
#include "config.h"
#include "pid_auto_tune.h"

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
    
    // Otomatik-ayarlama modu kontrolü
    void setAutoTuneMode(bool enabled);
    bool isAutoTuneEnabled() const;
    
    // Otomatik ayarlama tamamlandı mı?
    bool isAutoTuneFinished() const;
    
    // Otomatik ayarlama ilerleme yüzdesi
    int getAutoTuneProgress() const;
    
    // Otomatik ayarlamayı başlat
    void startAutoTune();

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