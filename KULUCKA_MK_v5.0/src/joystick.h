/**
 * @file joystick.h
 * @brief XY Joystick modülü yönetimi
 * @version 1.0
 */

#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <Arduino.h>
#include "config.h"

// Joystick yönleri
enum JoystickDirection {
    JOYSTICK_NONE,
    JOYSTICK_UP,
    JOYSTICK_DOWN,
    JOYSTICK_LEFT,
    JOYSTICK_RIGHT,
    JOYSTICK_PRESS
};

class Joystick {
public:
    // Yapılandırıcı
    Joystick();
    
    // Joystick modülünü başlat
    bool begin();
    
    // Joystick yönünü oku
    JoystickDirection readDirection();
    
    // Buton durumunu oku
    bool isButtonPressed();
    
    // Buton değişikliğini oku (yalnızca bir kez bastırma algılar)
    bool wasButtonPressed();
    
    // Joystick durumlarını güncelle (her döngüde çağrılmalı)
    void update();
    
    // Son basılma zamanını al
    unsigned long getLastActionTime();

private:
    // Joystick pozisyonları
    int _xPosition;
    int _yPosition;
    bool _buttonState;
    bool _lastButtonState;
    
    // Yön eşik değerleri
    const int _threshold = 1000; // 0-4095 aralığında, 1000 değeri temsili
    
    // Yön değişim zamanları
    unsigned long _lastDebounceTime;
    unsigned long _lastActionTime;
    unsigned long _lastJoystickRead; // Okumalar arasında minimum süre için
    
    // Debounce süresi (ms)
    const unsigned long _debounceDelay = 50;
    
    // Joystick kalibrasyonu
    int _xCenter;
    int _yCenter;
    
    // Kalibrasyonu oku
    void _calibrateJoystick();
    
    // Yön değişimi için debounce işlemi
    bool _debounceDirection(JoystickDirection newDirection);
    
    // Son yön
    JoystickDirection _lastDirection;
    JoystickDirection _currentDirection;
};

#endif // JOYSTICK_H