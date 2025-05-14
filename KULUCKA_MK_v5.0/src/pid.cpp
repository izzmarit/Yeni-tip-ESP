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
    
    // PID nesnesi başlangıçta NULL
    _pid = nullptr;
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

void PIDController::compute(double input) {
    _input = input;
    _lastError = _setpoint - _input;
    
    if (_pid != nullptr && _active) {
        _pid->Compute();
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
    // Sıcaklık hedef değerden _activationThreshold kadar düşükse ısıtıcı aktif
    // veya PID çıkışı 0.5'ten büyükse (bir tür PWM kontrolü için)
    return (_lastError >= _activationThreshold) || (_output > 0.5);
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