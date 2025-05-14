/**
 * @file menu.cpp
 * @brief Menü yönetimi uygulaması
 * @version 1.0
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
        {"PID", MENU_PID},
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
}

void MenuManager::update(JoystickDirection direction) {
    // Kullanıcı etkileşim zamanını güncelle
    updateInteractionTime();
    
    std::vector<MenuItem> currentItems = _getCurrentMenuItems();
    
    switch (direction) {
        case JOYSTICK_UP:
            if (_currentState == MENU_ADJUST_VALUE) {
                _adjustValue += _stepValue;
                if (_adjustValue > _maxValue) {
                    _adjustValue = _maxValue;
                }
            } else if (!currentItems.empty()) {
                _selectedIndex = (_selectedIndex > 0) ? _selectedIndex - 1 : currentItems.size() - 1;
            }
            break;
            
        case JOYSTICK_DOWN:
            if (_currentState == MENU_ADJUST_VALUE) {
                _adjustValue -= _stepValue;
                if (_adjustValue < _minValue) {
                    _adjustValue = _minValue;
                }
            } else if (!currentItems.empty()) {
                _selectedIndex = (_selectedIndex < currentItems.size() - 1) ? _selectedIndex + 1 : 0;
            }
            break;
            
        case JOYSTICK_RIGHT:
            if (_currentState == MENU_NONE) {
                // Ana ekrandan ana menüye geç
                _currentState = MENU_MAIN;
                _selectedIndex = 0;
            } else if (_currentState == MENU_ADJUST_VALUE) {
                // Değer ayarlama ekranında sağa basmak bir şey yapmasın
            } else if (!currentItems.empty() && _selectedIndex < currentItems.size()) {
                // Alt menüye git
                _previousState = _currentState;
                _currentState = currentItems[_selectedIndex].nextState;
                _selectedIndex = 0;
            }
            break;
            
        case JOYSTICK_LEFT:
            if (_currentState == MENU_MAIN) {
                // Ana menüden ana ekrana dön
                _currentState = MENU_NONE;
            } else if (_currentState == MENU_ADJUST_VALUE) {
                // Değer ayarlama ekranından bir önceki menüye dön
                _currentState = _previousState;
            } else {
                // Bir üst menüye dön (her durumda ana menüye)
                _currentState = MENU_MAIN;
                _selectedIndex = 0;
            }
            break;
            
        case JOYSTICK_PRESS:
            if (_currentState == MENU_ADJUST_VALUE) {
                // Değer onaylandı, önceki menü durumuna dön
                _currentState = _previousState;
            } else if (!currentItems.empty() && _selectedIndex < currentItems.size()) {
                MenuState nextState = currentItems[_selectedIndex].nextState;
                if (nextState != MENU_NONE) {
                    // Alt menüye git
                    _previousState = _currentState;
                    _currentState = nextState;
                    _selectedIndex = 0;
                }
                // MENU_NONE ise o özel işlem menü dışında işlenecek
            }
            break;
            
        case JOYSTICK_NONE:
            // Joystick durumunda değişiklik yok
            break;
    }
}

MenuState MenuManager::getCurrentState() const {
    return _currentState;
}

void MenuManager::returnToHome() {
    _currentState = MENU_NONE;
    _selectedIndex = 0;
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
}

float MenuManager::getAdjustedValue() const {
    return _adjustValue;
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
        return true;
    }
    
    return false;
}

bool MenuManager::goBack() {
    if (_currentState == MENU_MAIN) {
        // Ana menüden ana ekrana dön
        _currentState = MENU_NONE;
        return true;
    } else if (_currentState != MENU_NONE) {
        // Diğer menülerden ana menüye dön
        _currentState = MENU_MAIN;
        _selectedIndex = 0;
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
        default:
            return std::vector<MenuItem>(); // Boş liste
    }
}