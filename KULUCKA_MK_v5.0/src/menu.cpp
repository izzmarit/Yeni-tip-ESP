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
    
    // Kuluçka tipleri alt menüsü öğeleri
    _incubationTypeItems = {
        {"Tavuk", MENU_NONE},
        {"Bildircin", MENU_NONE},
        {"Kaz", MENU_NONE},
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
    
    // Debug için log çıktısı
    Serial.print("MenuManager::update - Yön: ");
    Serial.print(direction);
    Serial.print(", Mevcut Durum: ");
    Serial.println(_currentState);
    
    std::vector<MenuItem> currentItems = _getCurrentMenuItems();
    
    if (direction == JOYSTICK_NONE) {
        return; // Bir hareket yoksa işlem yapma
    }
    
    switch (direction) {
        case JOYSTICK_UP:
            if (_currentState == MENU_ADJUST_VALUE) {
                _adjustValue += _stepValue;
                if (_adjustValue > _maxValue) {
                    _adjustValue = _maxValue;
                }
                Serial.print("Değer arttı: ");
                Serial.println(_adjustValue);
            } else if (!currentItems.empty()) {
                _selectedIndex = (_selectedIndex > 0) ? _selectedIndex - 1 : currentItems.size() - 1;
                Serial.print("Seçim yukarı: ");
                Serial.println(_selectedIndex);
                _menuChanged = true; // Menünün değiştiğini işaretle
            }
            break;
            
        case JOYSTICK_DOWN:
            if (_currentState == MENU_ADJUST_VALUE) {
                _adjustValue -= _stepValue;
                if (_adjustValue < _minValue) {
                    _adjustValue = _minValue;
                }
                Serial.print("Değer azaldı: ");
                Serial.println(_adjustValue);
            } else if (!currentItems.empty()) {
                _selectedIndex = (_selectedIndex < currentItems.size() - 1) ? _selectedIndex + 1 : 0;
                Serial.print("Seçim aşağı: ");
                Serial.println(_selectedIndex);
                _menuChanged = true; // Menünün değiştiğini işaretle
            }
            break;
            
        case JOYSTICK_RIGHT:
            if (_currentState == MENU_NONE) {
                // Ana ekrandan ana menüye geç
                _currentState = MENU_MAIN;
                _selectedIndex = 0;
                _menuChanged = true; // Menünün değiştiğini işaretle
                Serial.println("Ana menüye geçildi (Sağ)");
            } else if (_currentState == MENU_ADJUST_VALUE) {
                // Değer ayarlama ekranında sağa basmak bir şey yapmasın
            } else if (!currentItems.empty() && _selectedIndex < currentItems.size()) {
                // Alt menüye git
                MenuState nextState = currentItems[_selectedIndex].nextState;
                
                // Sadece geçerli bir nextState varsa geçiş yap
                if (nextState != MENU_NONE) {
                    _previousState = _currentState;
                    _currentState = nextState;
                    _selectedIndex = 0;
                    _menuChanged = true; // Menünün değiştiğini işaretle
                    Serial.print("Alt menüye geçildi: ");
                    Serial.println(_currentState);
                }
            }
            break;
            
        case JOYSTICK_LEFT:
            if (_currentState == MENU_MAIN) {
                // Ana menüden ana ekrana dön
                _currentState = MENU_NONE;
                Serial.println("Ana ekrana dönüldü (Sol)");
                _menuChanged = true; // Menünün değiştiğini işaretle
            } else if (_currentState == MENU_ADJUST_VALUE) {
                // Değer ayarlama ekranından bir önceki menüye dön
                _currentState = _previousState;
                _menuChanged = true; // Menünün değiştiğini işaretle
                Serial.print("Değer ayardan geri dönüldü: ");
                Serial.println(_currentState);
            } else {
                // Bir üst menüye dön - önceki durumu daha iyi belirle
                if (_currentState == MENU_INCUBATION_TYPE || 
                    _currentState == MENU_TIME_DATE || 
                    _currentState == MENU_CALIBRATION || 
                    _currentState == MENU_ALARM || 
                    _currentState == MENU_MOTOR || 
                    _currentState == MENU_MANUAL_INCUBATION || 
                    _currentState == MENU_PID_MODE || 
                    _currentState == MENU_PID) {
                    // Bu durumlar ana menü altında, ana menüye dön
                    _currentState = MENU_MAIN;
                } else if (_currentState == MENU_ALARM_TEMP || 
                           _currentState == MENU_ALARM_HUMID) {
                    // Bu durumlar alarm alt menüsü altında, alarm menüsüne dön
                    _currentState = MENU_ALARM;
                } else if (_currentState == MENU_ALARM_TEMP_LOW || 
                           _currentState == MENU_ALARM_TEMP_HIGH) {
                    // Bu durumlar sıcaklık alarm alt menüsü altında, sıcaklık alarm menüsüne dön
                    _currentState = MENU_ALARM_TEMP;
                } else if (_currentState == MENU_ALARM_HUMID_LOW || 
                           _currentState == MENU_ALARM_HUMID_HIGH) {
                    // Bu durumlar nem alarm alt menüsü altında, nem alarm menüsüne dön
                    _currentState = MENU_ALARM_HUMID;
                } else {
                    // Diğer durumlarda ana menüye dön
                    _currentState = MENU_MAIN;
                }
                
                _selectedIndex = 0;
                _menuChanged = true; // Menünün değiştiğini işaretle
                Serial.print("Üst menüye dönüldü: ");
                Serial.println(_currentState);
            }
            break;
            
        case JOYSTICK_PRESS:
            if (_currentState == MENU_ADJUST_VALUE) {
                // Değer onaylandı, önceki menü durumuna dön
                _currentState = _previousState;
                _menuChanged = true; // Menünün değiştiğini işaretle
                Serial.println("Değer onaylandı");
            } else if (_currentState == MENU_NONE) {
                // Ana ekrandan ana menüye geç
                _currentState = MENU_MAIN;
                _selectedIndex = 0;
                _menuChanged = true; // Menünün değiştiğini işaretle
                Serial.println("Ana menüye geçildi (Basma)");
            } else if (!currentItems.empty() && _selectedIndex < currentItems.size()) {
                MenuState nextState = currentItems[_selectedIndex].nextState;
                if (nextState != MENU_NONE) {
                    // Alt menüye git
                    _previousState = _currentState;
                    _currentState = nextState;
                    _selectedIndex = 0;
                    _menuChanged = true; // Menünün değiştiğini işaretle
                    Serial.print("Alt menüye geçildi (Basma): ");
                    Serial.println(_currentState);
                } else {
                    Serial.println("Seçilen menü için işlem yok");
                }
                // MENU_NONE ise o özel işlem menü dışında işlenecek
            }
            break;
            
        default:
            // Joystick durumunda değişiklik yok
            break;
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