/**
 * @file pid.cpp
 * @brief PID (Proportional-Integral-Derivative) kontrolü uygulaması
 * @version 1.0
 */

#include "pid.h"

PIDController::PIDController() {
    _kp = PID_KP;
    _ki = PID_KI;
    _kd = PID_KD;
    _input = 0.0;
    _output = 0.0;
    _setpoint = 37.5; // Varsayılan hedef değer
    _active = false;
    _lastError = 0.0;
    _autoTuneMode = false;
    _heaterState = false;
    
    // PID nesnesi başlangıçta NULL
    _pid = nullptr;
}

PIDController::~PIDController() {
    // Bellek sızıntısını önlemek için PID nesnesini temizle
    if (_pid != nullptr) {
        delete _pid;
        _pid = nullptr;
    }
}

bool PIDController::begin() {
    // PID nesnesi oluştur
    _pid = new PID(&_input, &_output, &_setpoint, _kp, _ki, _kd, DIRECT);
    
    if (_pid == nullptr) {
        return false;
    }
    
    // PID ayarları
    _pid->SetOutputLimits(0, 1); // Çıkış 0-1 arasında olsun (0: kapalı, 1: açık)
    _pid->SetMode(AUTOMATIC);    // PID kontrolünü otomatik moda al
    _active = true;
    
    return true;
}

void PIDController::setTunings(double kp, double ki, double kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
    
    if (_pid != nullptr) {
        _pid->SetTunings(kp, ki, kd);
    }
}

void PIDController::setSetpoint(double setpoint) {
    _setpoint = setpoint;
}

void PIDController::setAutoTuneMode(bool enabled) {
    if (enabled == _autoTuneMode) {
        return; // Zaten istenen modda
    }
    
    _autoTuneMode = enabled;
    
    if (enabled) {
        // Otomatik ayarlama modunu başlat
        startAutoTune();
    } else {
        // Normal PID moduna geri dön
        // PID nesnesi zaten ayarlandıysa
        if (_pid != nullptr) {
            _pid->SetMode(AUTOMATIC);
        }
    }
}

bool PIDController::isAutoTuneEnabled() const {
    return _autoTuneMode;
}

bool PIDController::isAutoTuneFinished() const {
    return _autoTuner.isFinished();
}

int PIDController::getAutoTuneProgress() const {
    return _autoTuner.getProgress();
}

void PIDController::startAutoTune() {
    // Önceki PID modunu devre dışı bırak
    if (_pid != nullptr) {
        _pid->SetMode(MANUAL);
    }
    
    // Otomatik ayarlamayı başlat
    _autoTuner.start(_setpoint, &_input, &_heaterState);
}

void PIDController::compute(double input) {
    _input = input;
    _lastError = _setpoint - _input;
    
    if (_autoTuneMode) {
        // Otomatik ayarlama modunda
        _autoTuner.update();
        
        // Otomatik ayarlama tamamlandıysa PID parametrelerini güncelle
        if (_autoTuner.isFinished()) {
            _kp = _autoTuner.getKp();
            _ki = _autoTuner.getKi();
            _kd = _autoTuner.getKd();
            
            // PID parametrelerini güncelle
            if (_pid != nullptr) {
                _pid->SetTunings(_kp, _ki, _kd);
                _pid->SetMode(AUTOMATIC); // PID'yi otomatik moda al
            }
        }
        
        // Isıtıcı durumunu güncelle
        _output = _heaterState ? 1.0 : 0.0;
    } else {
        // Normal PID modu
        if (_pid != nullptr && _active) {
            _pid->Compute();
        }
    }
}

double PIDController::getOutput() const {
    return _output;
}

double PIDController::getKp() const {
    return _kp;
}

double PIDController::getKi() const {
    return _ki;
}

double PIDController::getKd() const {
    return _kd;
}

double PIDController::getSetpoint() const {
    return _setpoint;
}

bool PIDController::isOutputActive() const {
    if (_autoTuneMode) {
        return _heaterState;
    } else {
        // Sıcaklık hedef değerden _activationThreshold kadar düşükse ısıtıcı aktif
        // veya PID çıkışı 0.5'ten büyükse (bir tür PWM kontrolü için)
        return (_lastError >= _activationThreshold) || (_output > 0.5);
    }
}

void PIDController::setMode(bool active) {
    _active = active;
    
    if (_pid != nullptr) {
        _pid->SetMode(active ? AUTOMATIC : MANUAL);
    }
}

double PIDController::getError() const {
    return _lastError;
}