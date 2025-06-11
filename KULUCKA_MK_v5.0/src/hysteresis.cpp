/**
 * @file hysteresis.cpp
 * @brief Histerezis (On/Off) kontrol modülü uygulaması
 * @version 1.0
 */

#include "hysteresis.h"

Hysteresis::Hysteresis() {
    _setpoint = 60.0;      // Varsayılan hedef değer (%60 nem)
    _lowThreshold = 5.0;   // Varsayılan düşük eşik (%5)
    _highThreshold = 2.0;  // Varsayılan yüksek eşik (%2)
    _input = 0.0;
    _output = false;
    _lastDeviation = 0.0;
    
    // Başlangıç sınırlarını hesapla
    _calculateLimits();
}

bool Hysteresis::begin() {
    return true;
}

void Hysteresis::setSetpoint(double setpoint) {
    _setpoint = setpoint;
    _calculateLimits();
}

void Hysteresis::setLowThreshold(double threshold) {
    _lowThreshold = threshold;
    _calculateLimits();
}

void Hysteresis::setHighThreshold(double threshold) {
    _highThreshold = threshold;
    _calculateLimits();
}

void Hysteresis::compute(double input) {
    _input = input;
    _lastDeviation = _setpoint - _input;
    
    // Histerezis mantığı: 
    // 1. Eğer giriş değeri alt sınırın altına düşerse çıkış aktif olur.
    // 2. Eğer giriş değeri üst sınırın üzerine çıkarsa çıkış pasif olur.
    // 3. Bunun dışındaki durumlarda çıkışın durumu değişmez.
    
    if (_input <= _lowLimit) {
        _output = true;    // Nem düşük, nemlendiriciyi aktifleştir
    } else if (_input >= _highLimit) {
        _output = false;   // Nem yüksek, nemlendiriciyi devre dışı bırak
    }
    // Diğer durumda çıkış aynı kalır (histerezis bandı içinde)
}

bool Hysteresis::getOutput() const {
    return _output;
}

double Hysteresis::getSetpoint() const {
    return _setpoint;
}

double Hysteresis::getLowThreshold() const {
    return _lowThreshold;
}

double Hysteresis::getHighThreshold() const {
    return _highThreshold;
}

double Hysteresis::getDeviation() const {
    return _lastDeviation;
}

void Hysteresis::_calculateLimits() {
    // Alt ve üst sınırları hesapla
    _lowLimit = _setpoint - _lowThreshold;
    _highLimit = _setpoint + _highThreshold;
}