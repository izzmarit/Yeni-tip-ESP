/**
 * @file menu.cpp
 * @brief Menü yönetimi uygulaması
 * @version 1.1
 */

#include "menu.h"

MenuManager::MenuManager() {
    _currentState = MENU_NONE;
    _previousState = MENU_NONE;
    _selectedIndex = 0;
    _adjustValue = 0.0;
    _minValue = 0.0;
    _maxValue = 100.0;
    _stepValue = 1.0;
    _adjustTitle = "";
    _adjustUnit = "";
    _lastInteractionTime = 0;
    _menuChanged = true; // Başlangıçta menüyü çizmek için true yapıldı
    
    // Menü öğelerini başlat
    _initializeMenuItems();
}

bool MenuManager::begin() {
    return true;
}

void MenuManager::_initializeMenuItems() {
    // Ana menü öğeleri
    _mainMenuItems = {
        {"Kulucka Tipleri", MENU_INCUBATION_TYPE},
        {"Sicaklik", MENU_TEMPERATURE},
        {"Nem", MENU_HUMIDITY},
        {"PID Modu", MENU_PID_MODE},
        {"Motor", MENU_MOTOR},
        {"Saat ve Tarih", MENU_TIME_DATE},
        {"Kalibrasyon", MENU_CALIBRATION},
        {"Alarm", MENU_ALARM},
        {"WiFi Ayarlari", MENU_WIFI_SETTINGS}
    };
    
    // Kuluçka tipleri alt menüsü öğeleri - tüm öğelere geçerli bir nextState atanmış
    _incubationTypeItems = {
        {"Tavuk", MENU_MAIN},        // Kullanıcı bunu seçip onaylarsa MENU_MAIN durumuna dönecek
        {"Bildircin", MENU_MAIN},    // Burada MENU_NONE yerine MENU_MAIN kullanıldı
        {"Kaz", MENU_MAIN},          // Böylece seçim sonrası ana menüye dönecek
        {"Manuel", MENU_MANUAL_INCUBATION}
    };
    
    // Saat ve tarih alt menüsü öğeleri
    _timeDateItems = {
        {"Saati ayarla", MENU_SET_TIME},
        {"Tarihi ayarla", MENU_SET_DATE}
    };
    
    // Kalibrasyon alt menüsü öğeleri
    _calibrationItems = {
        {"Sicaklik Kalibrasyon", MENU_CALIBRATION_TEMP},
        {"Nem Kalibrasyon", MENU_CALIBRATION_HUMID}
    };
    
    // Alarm alt menüsü öğeleri
    _alarmItems = {
        {"Sicaklik Alarmlari", MENU_ALARM_TEMP},
        {"Nem Alarmlari", MENU_ALARM_HUMID},
        {"Motor Alarmlari", MENU_ALARM_MOTOR}
    };
    
    // Sıcaklık alarm alt menüsü öğeleri
    _tempAlarmItems = {
        {"Dusuk Sicaklik", MENU_ALARM_TEMP_LOW},
        {"Yuksek Sicaklik", MENU_ALARM_TEMP_HIGH}
    };
    
    // Nem alarm alt menüsü öğeleri
    _humidAlarmItems = {
        {"Dusuk Nem", MENU_ALARM_HUMID_LOW},
        {"Yuksek Nem", MENU_ALARM_HUMID_HIGH}
    };
    
    // Motor alt menüsü öğeleri
    _motorItems = {
        {"Bekleme Suresi", MENU_MOTOR_WAIT},
        {"Calisma Suresi", MENU_MOTOR_RUN}
    };
    
    // Manuel kuluçka alt menüsü öğeleri
    _manualIncubationItems = {
        {"Gelisim Sicakligi", MENU_MANUAL_DEV_TEMP},
        {"Cikim Sicakligi", MENU_MANUAL_HATCH_TEMP},
        {"Gelisim Nemi", MENU_MANUAL_DEV_HUMID},
        {"Cikim Nemi", MENU_MANUAL_HATCH_HUMID},
        {"Gelisim Gunleri", MENU_MANUAL_DEV_DAYS},
        {"Cikim Gunleri", MENU_MANUAL_HATCH_DAYS}
    };
    
    // PID ayarları alt menüsü öğeleri
    _pidItems = {
        {"Manuel PID", MENU_PID},
        {"Otomatik Ayarlama", MENU_PID_AUTO_TUNE}
    };
    
    // PID manuel ayar alt menüsü öğeleri
    _pidManualItems = {
        {"PID Kp", MENU_PID_KP},
        {"PID Ki", MENU_PID_KI},
        {"PID Kd", MENU_PID_KD}
    };
}

void MenuManager::update(JoystickDirection direction) {
    // Kullanıcı etkileşim zamanını güncelle
    updateInteractionTime();
    
    // Yön yok ise hiçbir şey yapma
    if (direction == JOYSTICK_NONE) {
        return;
    }
    
    // Debug için log çıktısı
    Serial.print("MenuManager::update - Yön: ");
    Serial.print(direction);
    Serial.print(", Mevcut Durum: ");
    Serial.println(_currentState);
    
    // Ana ekranda SADECE sağ yönde menüye giriş yap, diğer yönlerde hiçbir şey yapma
    if (_currentState == MENU_NONE) {
        if (direction == JOYSTICK_RIGHT) {
            // Ana ekrandan ana menüye geç (SADECE sağ yön için)
            _currentState = MENU_MAIN;
            _selectedIndex = 0;
            _menuChanged = true;
            Serial.println("Ana menüye geçildi");
        }
        return; // Ana ekranda diğer işlemleri yapma - ASLA
    }
    
    // Menüde iken işlemler
    if (_currentState == MENU_ADJUST_VALUE) {
        // Değer ayarlama ekranında
        switch (direction) {
            case JOYSTICK_UP:
                _adjustValue += _stepValue;
                if (_adjustValue > _maxValue) {
                    _adjustValue = _maxValue;
                }
                break;
                
            case JOYSTICK_DOWN:
                _adjustValue -= _stepValue;
                if (_adjustValue < _minValue) {
                    _adjustValue = _minValue;
                }
                break;
                
            case JOYSTICK_RIGHT:
                _adjustValue += _stepValue * 10;
                if (_adjustValue > _maxValue) {
                    _adjustValue = _maxValue;
                }
                break;
                
            case JOYSTICK_LEFT:
                // Değeri değiştirme yerine önceki menüye dön
                _currentState = _previousState;
                _menuChanged = true;
                break;
                
            case JOYSTICK_PRESS:
                // Değer onaylandı, önceki menü durumuna dön
                _currentState = _previousState;
                _menuChanged = true;
                break;
        }
    } else {
        // Normal menü ekranında
        switch (direction) {
            case JOYSTICK_UP:
                if (!_getCurrentMenuItems().empty()) {
                    _selectedIndex = (_selectedIndex > 0) ? _selectedIndex - 1 : _getCurrentMenuItems().size() - 1;
                    _menuChanged = true;
                }
                break;
                
            case JOYSTICK_DOWN:
                if (!_getCurrentMenuItems().empty()) {
                    _selectedIndex = (_selectedIndex < _getCurrentMenuItems().size() - 1) ? _selectedIndex + 1 : 0;
                    _menuChanged = true;
                }
                break;
                
            case JOYSTICK_RIGHT:
                if (!_getCurrentMenuItems().empty() && _selectedIndex < _getCurrentMenuItems().size()) {
                    MenuState nextState = _getCurrentMenuItems()[_selectedIndex].nextState;
                    
                    // Özel durumları ele al
                    if (_currentState == MENU_MAIN && _selectedIndex == 1) {
                        // Sıcaklık menüsü için özel işlem
                        _previousState = _currentState;
                        _currentState = MENU_TEMPERATURE;
                        _menuChanged = true;
                    } else if (_currentState == MENU_MAIN && _selectedIndex == 2) {
                        // Nem menüsü için özel işlem
                        _previousState = _currentState;
                        _currentState = MENU_HUMIDITY;
                        _menuChanged = true;
                    } else if (nextState != MENU_NONE) {
                        // Diğer menüler için normal alt menü geçişi
                        _previousState = _currentState;
                        _currentState = nextState;
                        _selectedIndex = 0;
                        _menuChanged = true;
                    }
                }
                break;
                
            case JOYSTICK_LEFT:
                // Menüden çıkış logikasını iyileştir - asla ana ekrana beklenmedik dönüş olmamalı
                if (_currentState == MENU_MAIN) {
                    // Ana menüden ana ekrana dön
                    _currentState = MENU_NONE;
                    _menuChanged = true;
                } else {
                    // Her alt menü için doğru üst menüye dön
                    MenuState newState = _previousState; // Önceki duruma dön
                    
                    // Eğer önceki durum geçersizse veya belli değilse, ana menüye dön
                    if (newState == MENU_NONE || newState == _currentState) {
                        newState = MENU_MAIN;
                    }
                    
                    _currentState = newState;
                    _selectedIndex = 0;
                    _menuChanged = true;
                }
                break;
                
            case JOYSTICK_PRESS:
                if (!_getCurrentMenuItems().empty() && _selectedIndex < _getCurrentMenuItems().size()) {
                    MenuState nextState = _getCurrentMenuItems()[_selectedIndex].nextState;
                    
                    // Özel durumları ele al
                    if (_currentState == MENU_MAIN && _selectedIndex == 1) {
                        // Sıcaklık menüsü için özel işlem
                        _previousState = _currentState;
                        _currentState = MENU_TEMPERATURE;
                        _menuChanged = true;
                    } else if (_currentState == MENU_MAIN && _selectedIndex == 2) {
                        // Nem menüsü için özel işlem
                        _previousState = _currentState;
                        _currentState = MENU_HUMIDITY;
                        _menuChanged = true;
                    } else if (nextState != MENU_NONE) {
                        // Diğer menüler için normal alt menü geçişi
                        _previousState = _currentState;
                        _currentState = nextState;
                        _selectedIndex = 0;
                        _menuChanged = true;
                    }
                }
                break;
        }
    }
}

MenuState MenuManager::getCurrentState() const {
    return _currentState;
}

MenuState MenuManager::getPreviousState() const {
    return _previousState;
}

void MenuManager::returnToHome() {
    _currentState = MENU_NONE;
    _selectedIndex = 0;
    _menuChanged = true; // Menünün değiştiğini işaretle
}

void MenuManager::showValueAdjustScreen(String title, float value, String unit, float minValue, float maxValue, float step) {
    _previousState = _currentState;
    _currentState = MENU_ADJUST_VALUE;
    _adjustValue = value;
    _minValue = minValue;
    _maxValue = maxValue;
    _stepValue = step;
    _adjustTitle = title;
    _adjustUnit = unit;
    _menuChanged = true; // Menünün değiştiğini işaretle
}

float MenuManager::getAdjustedValue() const {
    return _adjustValue;
}

String MenuManager::getAdjustTitle() const {
    return _adjustTitle;
}

String MenuManager::getAdjustUnit() const {
    return _adjustUnit;
}

void MenuManager::showConfirmation(String message) {
    // Bu fonksiyon display modülünün showConfirmationMessage fonksiyonunu çağıracak
    // Şimdilik sadece tanımlandı
}

std::vector<String> MenuManager::getMenuItems() const {
    std::vector<String> items;
    std::vector<MenuItem> currentItems = _getCurrentMenuItems();
    
    for (const MenuItem& item : currentItems) {
        items.push_back(item.name);
    }
    
    return items;
}

int MenuManager::getSelectedIndex() const {
    return _selectedIndex;
}

bool MenuManager::selectMenuItem(int index) {
    std::vector<MenuItem> currentItems = _getCurrentMenuItems();
    
    if (index >= 0 && index < currentItems.size()) {
        _selectedIndex = index;
        _menuChanged = true; // Menünün değiştiğini işaretle
        return true;
    }
    
    return false;
}

bool MenuManager::goBack() {
    if (_currentState == MENU_MAIN) {
        // Ana menüden ana ekrana dön
        _currentState = MENU_NONE;
        _menuChanged = true; // Menünün değiştiğini işaretle
        return true;
    } else if (_currentState != MENU_NONE) {
        // Diğer menülerden ana menüye dön
        _currentState = MENU_MAIN;
        _selectedIndex = 0;
        _menuChanged = true; // Menünün değiştiğini işaretle
        return true;
    }
    
    return false;
}

bool MenuManager::isInHomeScreen() const {
    return _currentState == MENU_NONE;
}

bool MenuManager::isInMenu() const {
    return _currentState != MENU_NONE && _currentState != MENU_ADJUST_VALUE;
}

bool MenuManager::isInValueAdjustScreen() const {
    return _currentState == MENU_ADJUST_VALUE;
}

unsigned long MenuManager::getLastInteractionTime() const {
    return _lastInteractionTime;
}

void MenuManager::updateInteractionTime() {
    _lastInteractionTime = millis();
}

void MenuManager::setCurrentState(MenuState state) {
    _previousState = _currentState;
    _currentState = state;
    _selectedIndex = 0; // Yeni menüye geçince ilk öğeyi seç
    _menuChanged = true; // Menünün değiştiğini işaretle
    updateInteractionTime(); // Kullanıcı etkileşim zamanını güncelle
}

void MenuManager::setSelectedIndex(int index) {
    // İndeksin geçerli aralıkta olduğundan emin ol
    std::vector<MenuItem> currentItems = _getCurrentMenuItems();
    
    if (!currentItems.empty() && index >= 0 && index < currentItems.size()) {
        _selectedIndex = index;
        _menuChanged = true; // Menünün değiştiğini işaretle
        updateInteractionTime(); // Kullanıcı etkileşim zamanını güncelle
    } else if (!currentItems.empty()) {
        _selectedIndex = 0; // Geçerli değilse, varsayılan olarak ilk öğeyi seç
        _menuChanged = true; // Menünün değiştiğini işaretle
    }
}

std::vector<MenuItem> MenuManager::_getCurrentMenuItems() const {
    switch (_currentState) {
        case MENU_MAIN:
            return _mainMenuItems;
        case MENU_INCUBATION_TYPE:
            return _incubationTypeItems;
        case MENU_TIME_DATE:
            return _timeDateItems;
        case MENU_CALIBRATION:
            return _calibrationItems;
        case MENU_ALARM:
            return _alarmItems;
        case MENU_ALARM_TEMP:
            return _tempAlarmItems;
        case MENU_ALARM_HUMID:
            return _humidAlarmItems;
        case MENU_MOTOR:
            return _motorItems;
        case MENU_MANUAL_INCUBATION:
            return _manualIncubationItems;
        case MENU_PID_MODE:
            return _pidItems;
        case MENU_PID:
            return _pidManualItems;
        default:
            return std::vector<MenuItem>(); // Boş liste
    }
}