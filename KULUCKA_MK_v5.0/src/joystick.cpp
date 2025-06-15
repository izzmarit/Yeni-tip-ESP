/**
 * @file joystick.cpp
 * @brief XY Joystick modülü yönetimi uygulaması
 * @version 1.0
 */

#include "joystick.h"

Joystick::Joystick() {
    _xPosition = 0;
    _yPosition = 0;
    _buttonState = HIGH;
    _lastButtonState = HIGH;
    _lastDebounceTime = 0;
    _lastActionTime = 0;
    _lastDirection = JOYSTICK_NONE;
    _currentDirection = JOYSTICK_NONE;
    _xCenter = 2048; // 12-bit ADC orta değeri
    _yCenter = 2048; // 12-bit ADC orta değeri
}

bool Joystick::begin() {
    // Joystick X ve Y analog pinlerini giriş olarak ayarla
    pinMode(JOY_X, INPUT);
    pinMode(JOY_Y, INPUT);
    
    // Joystick buton pinini giriş olarak ayarla ve dahili pull-up direnci etkinleştir
    pinMode(JOY_BTN, INPUT_PULLUP);
    
    // Joystick kalibrasyonu
    _calibrateJoystick();
    
    return true;
}

void Joystick::_calibrateJoystick() {
    // Başlangıçta orta değerleri ölç (birkaç örnek al ve ortalamasını hesapla)
    int sumX = 0;
    int sumY = 0;
    const int sampleCount = 10;
    
    for (int i = 0; i < sampleCount; i++) {
        sumX += analogRead(JOY_X);
        sumY += analogRead(JOY_Y);
        delay(10);
    }
    
    _xCenter = sumX / sampleCount;
    _yCenter = sumY / sampleCount;
    
    Serial.print("Joystick kalibre edildi - X merkez: ");
    Serial.print(_xCenter);
    Serial.print(", Y merkez: ");
    Serial.println(_yCenter);
}

JoystickDirection Joystick::readDirection() {
    JoystickDirection currentDir = _currentDirection;
    
    // Yönü okuduktan sonra sıfırla ki bir sonraki çağrıda eski değeri okumayalım
    if (currentDir != JOYSTICK_NONE) {
        _currentDirection = JOYSTICK_NONE;
    }
    
    return currentDir;
}

bool Joystick::isButtonPressed() {
    return (_buttonState == LOW); // Buton aktif düşük (LOW)
}

bool Joystick::wasButtonPressed() {
    // Buton yeni mi basıldı?
    bool result = (_buttonState == LOW) && (_lastButtonState == HIGH);
    return result;
}

void Joystick::update() {
    // Joystick değerlerini oku
    _xPosition = analogRead(JOY_X);
    _yPosition = analogRead(JOY_Y);
    
    // Mevcut zaman
    unsigned long currentTime = millis();
    
    // Buton durumunu oku
    bool reading = digitalRead(JOY_BTN);
    
    // Buton debounce işlemi
    if (reading != _lastButtonState) {
        _lastDebounceTime = currentTime;
    }
    
    if ((currentTime - _lastDebounceTime) > _debounceDelay) {
        if (reading != _buttonState) {
            _buttonState = reading;
            
            if (_buttonState == LOW) {
                // Buton basıldı
                _currentDirection = JOYSTICK_PRESS;
                _lastActionTime = currentTime;
                return; // Buton basıldığında hemen çık
            }
        }
    }
    
    _lastButtonState = reading;
    
    // Daha önce bir yön algılanmışsa ve son işlemden yeterince zaman geçmemişse çık
    if (_currentDirection != JOYSTICK_NONE) {
        return;
    }
    
    // Anti-bounce: Son hareketin üzerinden yeterli süre geçmemişse çık (500ms'ye artırıldı)
    if (currentTime - _lastActionTime < 500) {
        return;
    }
    
    // Yön belirleme
    JoystickDirection newDirection = JOYSTICK_NONE;
    
    // Ölü bölge eşiği - daha yüksek
    const int deadZoneThreshold = 1800;
    
    // X ekseni - merkeze göre oldukça farklıysa
    if (_xPosition < (_xCenter - deadZoneThreshold)) {
        newDirection = JOYSTICK_LEFT;
    } else if (_xPosition > (_xCenter + deadZoneThreshold)) {
        newDirection = JOYSTICK_RIGHT;
    }
    // Y ekseni - merkeze göre oldukça farklıysa (ve X hareket yok ise)
    else if (_yPosition < (_yCenter - deadZoneThreshold)) {
        newDirection = JOYSTICK_UP;
    } else if (_yPosition > (_yCenter + deadZoneThreshold)) {
        newDirection = JOYSTICK_DOWN;
    }
    
    // Eğer yeni bir yön algılandıysa
    if (newDirection != JOYSTICK_NONE) {
        _currentDirection = newDirection;
        _lastDirection = newDirection;
        _lastActionTime = currentTime;
    } else {
        // Eğer joystick merkeze dönmüşse _lastDirection'ı sıfırla
        if (abs(_xPosition - _xCenter) < (deadZoneThreshold / 2) && 
            abs(_yPosition - _yCenter) < (deadZoneThreshold / 2)) {
            _lastDirection = JOYSTICK_NONE;
        }
    }
}

unsigned long Joystick::getLastActionTime() {
    return _lastActionTime;
}