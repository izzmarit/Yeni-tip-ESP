/**
 * @file pid.cpp
 * @brief PID (Proportional-Integral-Derivative) kontrolü uygulaması
 * @version 1.2 - Mod geçişleri iyileştirildi ve güvenilirlik artırıldı
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
    _modeChangeTime = 0;
    _stabilizationTime = 2000; // 2 saniye stabilizasyon süresi
    
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
        Serial.println("PID: Nesne oluşturulamadı!");
        return false;
    }
    
    // PID ayarları
    _pid->SetOutputLimits(0, 1); // Çıkış 0-1 arasında olsun (0: kapalı, 1: açık)
    _pid->SetMode(MANUAL);       // Başlangıçta manuel modda başlat
    _active = false;
    _currentMode = PID_MODE_OFF;
    _modeChangeTime = millis();
    
    Serial.println("PID: Başlatıldı - Mod: Kapalı");
    return true;
}

void PIDController::setTunings(double kp, double ki, double kd) {
    // Parametre geçerliliğini kontrol et
    if (kp < 0 || ki < 0 || kd < 0) {
        Serial.println("PID: Geçersiz parametre değerleri!");
        return;
    }
    
    _kp = kp;
    _ki = ki;
    _kd = kd;
    
    if (_pid != nullptr) {
        _pid->SetTunings(kp, ki, kd);
        Serial.println("PID: Parametreler güncellendi - Kp:" + String(kp) + 
                      " Ki:" + String(ki) + " Kd:" + String(kd));
    }
}

void PIDController::setSetpoint(double setpoint) {
    if (setpoint < 0 || setpoint > 50) {
        Serial.println("PID: Geçersiz setpoint değeri: " + String(setpoint));
        return;
    }
    
    _setpoint = setpoint;
    Serial.println("PID: Hedef sıcaklık: " + String(setpoint) + "°C");
}

void PIDController::setPIDMode(PIDMode mode) {
    if (_currentMode == mode) {
        Serial.println("PID: Zaten " + _getModeString(mode) + " modunda");
        return; // Zaten istenen modda
    }
    
    PIDMode oldMode = _currentMode;
    _currentMode = mode;
    _modeChangeTime = millis();
    
    Serial.println("PID: Mod değişimi: " + _getModeString(oldMode) + " -> " + _getModeString(mode));
    
    switch (mode) {
        case PID_MODE_OFF:
            _stopPID();
            break;
            
        case PID_MODE_MANUAL:
            _stopAutoTune(); // Otomatik ayarlama varsa durdur
            _startManualPID();
            break;
            
        case PID_MODE_AUTO_TUNE:
            _stopManualPID(); // Manuel PID varsa durdur
            _startAutoTune();
            break;
    }
}

PIDMode PIDController::getPIDMode() const {
    return _currentMode;
}

void PIDController::startManualMode() {
    setPIDMode(PID_MODE_MANUAL);
}

void PIDController::setAutoTuneMode(bool enabled) {
    if (enabled) {
        setPIDMode(PID_MODE_AUTO_TUNE);
    } else {
        // Otomatik ayarlama bittiğinde manuel moda geç
        if (_currentMode == PID_MODE_AUTO_TUNE) {
            setPIDMode(PID_MODE_MANUAL);
        }
    }
}

bool PIDController::isAutoTuneEnabled() const {
    return _currentMode == PID_MODE_AUTO_TUNE && _autoTuneMode;
}

bool PIDController::isAutoTuneFinished() const {
    return _autoTuner.isFinished();
}

int PIDController::getAutoTuneProgress() const {
    if (_currentMode != PID_MODE_AUTO_TUNE) {
        return 0;
    }
    return _autoTuner.getProgress();
}

void PIDController::startAutoTune() {
    setPIDMode(PID_MODE_AUTO_TUNE);
}

void PIDController::compute(double input) {
    _input = input;
    _lastError = _setpoint - _input;
    
    // Mod değişiminden sonra stabilizasyon süresi
    if (millis() - _modeChangeTime < _stabilizationTime) {
        return; // Stabilizasyon süresince hesaplama yapma
    }
    
    if (_currentMode == PID_MODE_AUTO_TUNE && _autoTuneMode) {
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
            
            Serial.println("PID: Otomatik ayarlama tamamlandı:");
            Serial.println("  Kp: " + String(_kp));
            Serial.println("  Ki: " + String(_ki));
            Serial.println("  Kd: " + String(_kd));
            
            // Otomatik ayarlama bittikten sonra manuel moda geç
            setAutoTuneMode(false);
        }
        
        // Isıtıcı durumunu güncelle
        _output = _heaterState ? 1.0 : 0.0;
        
    } else if (_currentMode == PID_MODE_MANUAL && _active && _pid != nullptr) {
        // Normal manuel PID modu
        _pid->Compute();
        
    } else {
        // PID kapalı - çıkışı sıfırla
        _output = 0.0;
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
    if (_currentMode == PID_MODE_OFF) {
        return false; // PID kapalıysa ısıtıcı da kapalı
    }
    
    if (_currentMode == PID_MODE_AUTO_TUNE) {
        return _heaterState;
    } 
    
    if (_currentMode == PID_MODE_MANUAL && _active) {
        // Manuel modda: PID çıkışı veya sıcaklık farkına göre
        return (_output > 0.5) || (_lastError >= _activationThreshold);
    }
    
    return false;
}

void PIDController::setMode(bool active) {
    if (_currentMode == PID_MODE_OFF) {
        Serial.println("PID: Kapalı modda setMode() çağrısı göz ardı edildi");
        return;
    }
    
    _active = active;
    
    if (_pid != nullptr) {
        _pid->SetMode(active ? AUTOMATIC : MANUAL);
    }
    
    Serial.println("PID: Durum değişti - " + String(active ? "Aktif" : "Pasif"));
}

double PIDController::getError() const {
    return _lastError;
}

String PIDController::getPIDModeString() const {
    return _getModeString(_currentMode);
}

bool PIDController::isManualModeActive() const {
    return (_currentMode == PID_MODE_MANUAL && _active);
}

// *** YENİ PRIVATE METODLAR ***

void PIDController::_stopPID() {
    _active = false;
    _autoTuneMode = false;
    
    if (_pid != nullptr) {
        _pid->SetMode(MANUAL);
    }
    
    _output = 0.0;
    _heaterState = false;
    
    Serial.println("PID: Tüm PID işlemleri durduruldu");
}

void PIDController::_startManualPID() {
    _stopAutoTune(); // Önce otomatik ayarlamayı durdur
    
    _active = true;
    _autoTuneMode = false;
    
    if (_pid != nullptr) {
        _pid->SetMode(AUTOMATIC);
        Serial.println("PID: Manuel mod başlatıldı");
    } else {
        Serial.println("PID: Manuel mod başlatılamadı - PID nesnesi yok!");
        _active = false;
    }
}

void PIDController::_startAutoTune() {
    _stopManualPID(); // Önce manuel PID'yi durdur
    
    _active = true;
    _autoTuneMode = true;
    
    // PID'yi manuel moda al (otomatik ayarlama kendisi kontrol edecek)
    if (_pid != nullptr) {
        _pid->SetMode(MANUAL);
    }
    
    // Otomatik ayarlamayı başlat
    _autoTuner.start(_setpoint, &_input, &_heaterState);
    
    Serial.println("PID: Otomatik ayarlama başlatıldı - Hedef: " + String(_setpoint) + "°C");
}

void PIDController::_stopManualPID() {
    if (_currentMode == PID_MODE_MANUAL) {
        _active = false;
        if (_pid != nullptr) {
            _pid->SetMode(MANUAL);
        }
        Serial.println("PID: Manuel mod durduruldu");
    }
}

void PIDController::_stopAutoTune() {
    if (_autoTuneMode) {
        _autoTuneMode = false;
        _autoTuner.cancel();
        _heaterState = false;
        Serial.println("PID: Otomatik ayarlama durduruldu");
    }
}

String PIDController::_getModeString(PIDMode mode) const {
    switch (mode) {
        case PID_MODE_OFF:
            return "Kapalı";
        case PID_MODE_MANUAL:
            return _active ? "Manuel Aktif" : "Manuel Beklemede";
        case PID_MODE_AUTO_TUNE:
            return "Otomatik Ayarlama";
        default:
            return "Bilinmeyen";
    }
}