/**
 * @file pid_auto_tune.cpp
 * @brief PID Otomatik Ayarlama Uygulaması
 * @version 1.0
 */

#include "pid_auto_tune.h"

PIDAutoTune::PIDAutoTune() {
    _input = NULL;
    _output = NULL;
    _setpoint = 0;
    _state = CANCELED;
    _kp = 0;
    _ki = 0;
    _kd = 0;
    _peak1 = 0;
    _peak2 = 0;
    _lastPeak = 0;
    _peakTimestamp1 = 0;
    _peakTimestamp2 = 0;
    _startTime = 0;
    _period = 0;
    _amplitude = 0;
    _Ku = 0;
    _Tu = 0;
    _lastOnTime = 0;
    _lastOffTime = 0;
    _currentTime = 0;
    _lastInput = 0;
    _progress = 0;
    _lastSafetyCheckTime = 0;
    _maxTemperature = 0;
    _minTemperature = 0;
}

void PIDAutoTune::start(double setpoint, double *input, bool *output) {
    if (input == NULL || output == NULL) {
        return;
    }
    
    _input = input;
    _output = output;
    _setpoint = setpoint;
    _state = INIT;
    _peak1 = 0;
    _peak2 = 0;
    _lastPeak = 0;
    _peakTimestamp1 = 0;
    _peakTimestamp2 = 0;
    _startTime = millis();
    _lastInput = *_input;
    _progress = 0;
    _lastSafetyCheckTime = millis();
    
    // Güvenlik sınırlarını ayarla
    _maxTemperature = _setpoint + 2.0; // En fazla 2°C üstüne çıkmasına izin ver
    _minTemperature = _setpoint - 5.0; // En fazla 5°C altına düşmesine izin ver
    
    // İlk olarak ısıtıcıyı aç
    *_output = true;
}

void PIDAutoTune::cancel() {
    _state = CANCELED;
    _progress = 0;
}

void PIDAutoTune::update() {
    if (_state == FINISHED || _state == CANCELED) {
        return;
    }
    
    _currentTime = millis();
    
    // Güvenlik kontrolü - her 5 saniyede bir
    if (_currentTime - _lastSafetyCheckTime > 5000) {
        _lastSafetyCheckTime = _currentTime;
        
        // Sıcaklık çok yüksekse ısıtıcıyı kapat
        if (*_input >= _maxTemperature) {
            *_output = false;
            Serial.println("Otomatik Ayarlama: Sıcaklık güvenlik sınırına ulaştı, ısıtıcı kapatıldı");
        }
        
        // Sıcaklık çok düşükse ısıtıcıyı aç
        if (*_input <= _minTemperature) {
            *_output = true;
            Serial.println("Otomatik Ayarlama: Sıcaklık güvenlik sınırının altına düştü, ısıtıcı açıldı");
        }
    }
    
    // Zaman aşımı kontrolü (30 dakika)
    if (_currentTime - _startTime > 1800000) {
        Serial.println("Otomatik Ayarlama: Zaman aşımı (30 dakika)");
        cancel();
        return;
    }
    
    // İlerleme yüzdesini güncelle
    unsigned long elapsedTime = _currentTime - _startTime;
    _progress = min(95, (int)(elapsedTime / 18000)); // Maksimum 30 dakika, %95'e kadar
    
    switch (_state) {
        case INIT:
            // Sıcaklık artışını bekle ve ilk ayarları yap
            if (*_input >= _setpoint + 0.5) {
                // İlk salınım için ısıtıcıyı kapat
                *_output = false;
                _state = WAITING_FOR_PEAK1;
                Serial.println("Otomatik Ayarlama: İlk salınıma ulaşıldı, ısıtıcı kapatıldı");
            }
            break;
            
        case WAITING_FOR_PEAK1:
            // İlk tepe noktasını bekle
            if (_isPeak()) {
                _peak1 = *_input;
                _peakTimestamp1 = _currentTime;
                _state = WAITING_FOR_PEAK2;
                Serial.println("Otomatik Ayarlama: İlk tepe noktası tespit edildi: " + String(_peak1));
            }
            
            // Isıtıcı kontrolü
            if (*_input <= _setpoint - 0.5) {
                *_output = true;  // Aşağı inerse ısıtıcıyı aç
            } else if (*_input >= _setpoint + 0.5) {
                *_output = false; // Yukarı çıkarsa ısıtıcıyı kapat
            }
            break;
            
        case WAITING_FOR_PEAK2:
            // İkinci tepe noktasını bekle
            if (_isPeak()) {
                _peak2 = *_input;
                _peakTimestamp2 = _currentTime;
                
                // Periyot ve genliği hesapla
                _period = (_peakTimestamp2 - _peakTimestamp1) / 1000.0; // saniye
                _amplitude = (_peak1 + _peak2) / 2.0 - _setpoint;
                
                Serial.println("Otomatik Ayarlama: İkinci tepe noktası tespit edildi: " + String(_peak2));
                Serial.println("Otomatik Ayarlama: Periyot = " + String(_period) + "s, Genlik = " + String(_amplitude) + "°C");
                
                // Isıtıcı sürelerine göre Ku hesapla
                _Ku = 4.0 * _amplitude / (3.14159 * _amplitude);
                _Tu = _period;
                
                // PID parametrelerini hesapla
                _calculateZieglerNichols();
                
                Serial.println("Otomatik Ayarlama: Yeni PID değerleri - Kp: " + String(_kp) + ", Ki: " + String(_ki) + ", Kd: " + String(_kd));
                
                _state = FINISHED;
                _progress = 100;
            }
            
            // Isıtıcı kontrolü
            if (*_input <= _setpoint - 0.5) {
                *_output = true;  // Aşağı inerse ısıtıcıyı aç
            } else if (*_input >= _setpoint + 0.5) {
                *_output = false; // Yukarı çıkarsa ısıtıcıyı kapat
            }
            break;
            
        default:
            break;
    }
    
    _lastInput = *_input;
}

bool PIDAutoTune::isFinished() const {
    return _state == FINISHED;
}

double PIDAutoTune::getKp() const {
    return _kp;
}

double PIDAutoTune::getKi() const {
    return _ki;
}

double PIDAutoTune::getKd() const {
    return _kd;
}

void PIDAutoTune::setLastOnTime(unsigned long time) {
    _lastOnTime = time;
}

void PIDAutoTune::setLastOffTime(unsigned long time) {
    _lastOffTime = time;
}

int PIDAutoTune::getProgress() const {
    return _progress;
}

bool PIDAutoTune::_isPeak() {
    // Gelişmiş tepe noktası tespiti
    static bool risingTemp = true;
    static float lastInputChange = 0;
    static float lastLastInput = 0;
    static int steadyCount = 0;
    
    float inputChange = *_input - _lastInput;
    
    // Yön değişimi tespit et
    if (risingTemp && inputChange < -0.05) { // Yüksekten düşüşe geçiş (tepe)
        risingTemp = false;
        steadyCount = 0;
        
        // Önceki değişim yukarı doğruysa (gerçek tepe)
        if (lastInputChange > 0) {
            return true;
        }
    } else if (!risingTemp && inputChange > 0.05) { // Düşükten yükselişe geçiş (vadi)
        risingTemp = true;
        steadyCount = 0;
    } else if (abs(inputChange) < 0.03) { // Sabit durum
        steadyCount++;
        
        // 5 kere sabit durumda kalırsa, yön değişimi olarak kabul et
        if (steadyCount > 5) {
            if (risingTemp && lastInputChange < 0) {
                risingTemp = false;
                steadyCount = 0;
                return true;
            } else if (!risingTemp && lastInputChange > 0) {
                risingTemp = true;
                steadyCount = 0;
            }
        }
    } else {
        steadyCount = 0;
    }
    
    // Değişim miktarlarını güncelle
    lastLastInput = _lastInput;
    lastInputChange = inputChange;
    
    return false;
}

void PIDAutoTune::_calculateZieglerNichols() {
    // Ziegler-Nichols metodu (classic PID)
    _kp = 0.6 * _Ku;
    _ki = 1.2 * _Ku / _Tu;
    _kd = 0.075 * _Ku * _Tu;
    
    // Sınırlar
    _kp = constrain(_kp, 0.1, 100.0);
    _ki = constrain(_ki, 0.01, 10.0);
    _kd = constrain(_kd, 0.1, 100.0);
}

double PIDAutoTune::getMaxTemperature() const {
    return _maxTemperature;
}

double PIDAutoTune::getMinTemperature() const {
    return _minTemperature;
}