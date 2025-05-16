/**
 * @file joystick.cpp
 * @brief XY Joystick modülü yönetimi uygulaması
 * @version 1.0
 */

#include "joystick.h"

Joystick::Joystick() {
    _xPosition = 0;
    _yPosition = 0;
    _buttonState = false;
    _lastButtonState = false;
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
}

JoystickDirection Joystick::readDirection() {
    return _currentDirection;
}

bool Joystick::isButtonPressed() {
    return !_buttonState; // Buton aktif düşük (LOW)
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
    
    // Buton durumunu oku
    bool reading = digitalRead(JOY_BTN);
    
    // Buton debounce işlemi
    if (reading != _lastButtonState) {
        _lastDebounceTime = millis();
    }
    
    if ((millis() - _lastDebounceTime) > _debounceDelay) {
        if (reading != _buttonState) {
            _buttonState = reading;
            
            if (_buttonState == LOW) {
                // Buton basıldı
                _currentDirection = JOYSTICK_PRESS;
                _lastActionTime = millis();
            }
        }
    }
    
    _lastButtonState = reading;
    
    // Yön belirleme
    JoystickDirection newDirection = JOYSTICK_NONE;
    
    // X değeri merkeze göre oldukça farklıysa
    if (_xPosition < (_xCenter - _threshold)) {
        newDirection = JOYSTICK_LEFT;
    } else if (_xPosition > (_xCenter + _threshold)) {
        newDirection = JOYSTICK_RIGHT;
    }
    // Y değeri merkeze göre oldukça farklıysa (ve X hareket yok ise)
    else if (_yPosition < (_yCenter - _threshold)) {
        newDirection = JOYSTICK_DOWN;
    } else if (_yPosition > (_yCenter + _threshold)) {
        newDirection = JOYSTICK_UP;
    }
    
    // Eğer buton basılı değilse ve yeni bir yön algılandıysa
    if (_currentDirection != JOYSTICK_PRESS && newDirection != JOYSTICK_NONE) {
        if (_debounceDirection(newDirection)) {
            _currentDirection = newDirection;
            _lastActionTime = millis();
        }
    }
    
    // Eğer joystick merkeze dönmüşse yönü sıfırla
    if (newDirection == JOYSTICK_NONE && 
        abs(_xPosition - _xCenter) < (_threshold / 2) && 
        abs(_yPosition - _yCenter) < (_threshold / 2)) {
        _lastDirection = JOYSTICK_NONE;
        _currentDirection = JOYSTICK_NONE;
    }
}

bool Joystick::_debounceDirection(JoystickDirection newDirection) {
    unsigned long currentTime = millis();
    
    // Aynı yönde ise ve belirli bir süre geçtiyse
    if (newDirection == _lastDirection && (currentTime - _lastDebounceTime) > (_debounceDelay * 2)) {
        return true;
    }
    // Farklı yönde ise
    else if (newDirection != _lastDirection) {
        _lastDirection = newDirection;
        _lastDebounceTime = currentTime;
        return true;
    }
    
    return false;
}

unsigned long Joystick::getLastActionTime() {
    return _lastActionTime;
}