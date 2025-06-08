/**
 * @file pid.cpp
 * @brief PID (Proportional-Integral-Derivative) kontrolü uygulaması
 * @version 1.1
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
    _currentMode = PID_MODE_OFF;
    
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
    _pid->SetMode(MANUAL);       // Başlangıçta manuel modda başlat
    _active = false;
    _currentMode = PID_MODE_OFF;
    
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

void PIDController::setPIDMode(PIDMode mode) {
    if (_currentMode == mode) {
        return; // Zaten istenen modda
    }
    
    _currentMode = mode;
    
    switch (mode) {
        case PID_MODE_OFF:
            // PID'yi kapat
            if (_pid != nullptr) {
                _pid->SetMode(MANUAL);
            }
            _active = false;
            _autoTuneMode = false;
            Serial.println("PID modu kapatıldı");
            break;
            
        case PID_MODE_MANUAL:
            // Manuel PID moduna geç ve aktifleştir - DÜZELTME
            _autoTuneMode = false;
            if (_pid != nullptr) {
                _pid->SetMode(AUTOMATIC);
            }
            _active = true;
            Serial.println("Manuel PID modu aktifleştirildi");
            break;
            
        case PID_MODE_AUTO_TUNE:
            // Otomatik ayarlama modunu başlat
            setAutoTuneMode(true);
            break;
    }
}

PIDMode PIDController::getPIDMode() const {
    return _currentMode;
}

void PIDController::startManualMode() {
    if (_currentMode != PID_MODE_MANUAL) {
        setPIDMode(PID_MODE_MANUAL);
    }
    
    // Manuel PID'yi aktif et
    if (_pid != nullptr) {
        _pid->SetMode(AUTOMATIC);
    }
    _active = true;
    _autoTuneMode = false;
    
    Serial.println("Manuel PID modu başlatıldı");
}

void PIDController::setAutoTuneMode(bool enabled) {
    if (enabled == _autoTuneMode) {
        return; // Zaten istenen modda
    }
    
    _autoTuneMode = enabled;
    
    if (enabled) {
        // Otomatik ayarlama modunu başlat
        _currentMode = PID_MODE_AUTO_TUNE;
        startAutoTune();
    } else {
        // Normal PID moduna geri dön
        if (_currentMode == PID_MODE_AUTO_TUNE) {
            _currentMode = PID_MODE_MANUAL;
            startManualMode(); // Otomatik ayarlama bittikten sonra manuel moda geç
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
    
    _active = true; // Otomatik ayarlama sırasında aktif
    
    // Otomatik ayarlamayı başlat
    _autoTuner.start(_setpoint, &_input, &_heaterState);
    
    Serial.println("PID Otomatik Ayarlama başlatıldı");
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
            }
            
            Serial.println("Otomatik ayarlama tamamlandı:");
            Serial.println("Kp: " + String(_kp));
            Serial.println("Ki: " + String(_ki));
            Serial.println("Kd: " + String(_kd));
            
            // Otomatik ayarlama bittikten sonra manuel moda geç
            setAutoTuneMode(false);
        }
        
        // Isıtıcı durumunu güncelle
        _output = _heaterState ? 1.0 : 0.0;
    } else {
        // Normal PID modu
        if (_pid != nullptr && _active && _currentMode == PID_MODE_MANUAL) {
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
    } else if (_active && _currentMode == PID_MODE_MANUAL) {
        // Sıcaklık hedef değerden _activationThreshold kadar düşükse ısıtıcı aktif
        // veya PID çıkışı 0.5'ten büyükse (bir tür PWM kontrolü için)
        return (_lastError >= _activationThreshold) || (_output > 0.5);
    } else {
        return false; // PID kapalıysa ısıtıcı da kapalı
    }
}

void PIDController::setMode(bool active) {
    _active = active;
    
    if (_pid != nullptr) {
        _pid->SetMode(active ? AUTOMATIC : MANUAL);
    }
    
    if (!active) {
        _currentMode = PID_MODE_OFF;
    }
}

double PIDController::getError() const {
    return _lastError;
}

String PIDController::getPIDModeString() const {
    switch (_currentMode) {
        case PID_MODE_OFF:
            return "Kapalı";
        case PID_MODE_MANUAL:
            if (_active) {
                return "Manuel (Aktif)";
            } else {
                return "Manuel (Beklemede)";
            }
        case PID_MODE_AUTO_TUNE:
            return "Otomatik Ayarlama";
        default:
            return "Bilinmeyen";
    }
}

bool PIDController::isManualModeActive() const {
    return (_currentMode == PID_MODE_MANUAL && _active);
}