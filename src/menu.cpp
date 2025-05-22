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
    
    // Motor alt menüsü öğeleri
    _motorItems = {
        {"Bekleme Suresi", MENU_MOTOR_WAIT},
        {"Calisma Suresi", MENU_MOTOR_RUN}
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
    updateInteractionTime();
    
    if (direction == JOYSTICK_NONE) {
        return;
    }
    
    Serial.print("MenuManager::update - Yön: ");
    Serial.print(direction);
    Serial.print(", Mevcut Durum: ");
    Serial.println(_currentState);
    
    // Ana ekrandan menüye giriş (bu kontrol main.cpp'de yapılıyor artık)
    if (_currentState == MENU_NONE) {
        return; // Ana ekran kontrolleri main.cpp'de yapılıyor
    }
    
    // Değer ayarlama ekranında
    if (_currentState == MENU_ADJUST_VALUE) {
        switch (direction) {
            case JOYSTICK_UP:
                _adjustValue += _stepValue;
                if (_adjustValue > _maxValue) _adjustValue = _maxValue;
                break;
            case JOYSTICK_DOWN:
                _adjustValue -= _stepValue;
                if (_adjustValue < _minValue) _adjustValue = _minValue;
                break;
            case JOYSTICK_LEFT:
                // Değer ayarlama ekranından geri dön
                _currentState = _previousState;
                _menuChanged = true;
                Serial.println("Değer ayarlama ekranından geri dönüldü");
                break;
            case JOYSTICK_RIGHT:
                _adjustValue += _stepValue * 10;
                if (_adjustValue > _maxValue) _adjustValue = _maxValue;
                break;
            case JOYSTICK_PRESS:
                // Değer kaydedilecek - main.cpp'de işlenecek
                break;
        }
        return;
    }
    
    // Terminal menüler (ayar ekranları) - sadece geri dönüş izni
    if (_isTerminalMenu(_currentState)) {
        if (direction == JOYSTICK_LEFT) {
            MenuState backState = _getBackState(_currentState);
            _currentState = backState;
            _selectedIndex = 0;
            _menuChanged = true;
            Serial.print("Terminal menüden geri dönüş: ");
            Serial.println(backState);
        }
        // Terminal menülerde sağ yön ve buton main.cpp'de işleniyor
        return;
    }
    
    // Normal menü navigasyonu
    std::vector<MenuItem> currentItems = _getCurrentMenuItems();
    
    if (currentItems.empty()) {
        if (_currentState != MENU_MAIN) {
            _currentState = MENU_MAIN;
            _selectedIndex = 0;
            _menuChanged = true;
        }
        return;
    }
    
    switch (direction) {
        case JOYSTICK_UP:
            _selectedIndex = (_selectedIndex > 0) ? _selectedIndex - 1 : currentItems.size() - 1;
            _menuChanged = true;
            break;
            
        case JOYSTICK_DOWN:
            _selectedIndex = (_selectedIndex < currentItems.size() - 1) ? _selectedIndex + 1 : 0;
            _menuChanged = true;
            break;
            
        case JOYSTICK_RIGHT:
            if (_selectedIndex < currentItems.size()) {
                MenuState nextState = currentItems[_selectedIndex].nextState;
                if (nextState != MENU_NONE) {
                    _previousState = _currentState;
                    _currentState = nextState;
                    _selectedIndex = 0;
                    _menuChanged = true;
                    Serial.print("Menü geçişi: ");
                    Serial.println(nextState);
                }
            }
            break;
            
        case JOYSTICK_LEFT:
            // ANA MENÜDEN ANA EKRANA DÖNÜŞ KRİTİK DEĞİŞİKLİK!
            if (_currentState == MENU_MAIN) {
                // Ana menüden ana ekrana dönüş için çift tıklama gerekiyor
                static unsigned long lastLeftPressTime = 0;
                unsigned long currentTime = millis();
                
                if (currentTime - lastLeftPressTime < 1000) { // 1 saniye içinde çift tıklama
                    _currentState = MENU_NONE;
                    _menuChanged = true;
                    Serial.println("Çift tıklama ile ana ekrana dönüldü");
                    lastLeftPressTime = 0; // Reset
                } else {
                    lastLeftPressTime = currentTime;
                    Serial.println("Ana ekrana dönmek için tekrar sola çekin");
                }
            } else {
                // Diğer menülerden geri dönüş
                MenuState backState = _getBackState(_currentState);
                _currentState = backState;
                _selectedIndex = 0;
                _menuChanged = true;
                Serial.print("Geri dönüş: ");
                Serial.println(backState);
            }
            break;
            
        case JOYSTICK_PRESS:
            // Buton işlemleri main.cpp'de yapılacak
            break;
    }
}

bool MenuManager::_isTerminalMenu(MenuState state) const {
    switch (state) {
        case MENU_TEMPERATURE:
        case MENU_HUMIDITY:
        case MENU_PID_KP:
        case MENU_PID_KI:
        case MENU_PID_KD:
        case MENU_PID_AUTO_TUNE:
        case MENU_MOTOR_WAIT:
        case MENU_MOTOR_RUN:
        case MENU_SET_TIME:
        case MENU_SET_DATE:
        case MENU_CALIBRATION_TEMP:
        case MENU_CALIBRATION_HUMID:
        case MENU_ALARM_TEMP_LOW:
        case MENU_ALARM_TEMP_HIGH:
        case MENU_ALARM_HUMID_LOW:
        case MENU_ALARM_HUMID_HIGH:
        case MENU_ALARM_MOTOR:
        case MENU_MANUAL_DEV_TEMP:
        case MENU_MANUAL_HATCH_TEMP:
        case MENU_MANUAL_DEV_HUMID:
        case MENU_MANUAL_HATCH_HUMID:
        case MENU_MANUAL_DEV_DAYS:
        case MENU_MANUAL_HATCH_DAYS:
            return true;
        default:
            return false;
    }
}

MenuState MenuManager::_getBackState(MenuState currentState) {
    switch (currentState) {
        case MENU_INCUBATION_TYPE:
        case MENU_PID_MODE:
        case MENU_MOTOR:
        case MENU_TIME_DATE:
        case MENU_CALIBRATION:
        case MENU_ALARM:
            return MENU_MAIN;
            
        case MENU_PID:
        case MENU_PID_AUTO_TUNE:
            return MENU_PID_MODE;
            
        case MENU_MOTOR_WAIT:
        case MENU_MOTOR_RUN:
            return MENU_MOTOR;
            
        case MENU_SET_TIME:
        case MENU_SET_DATE:
            return MENU_TIME_DATE;
            
        case MENU_CALIBRATION_TEMP:
        case MENU_CALIBRATION_HUMID:
            return MENU_CALIBRATION;
            
        case MENU_ALARM_TEMP:
        case MENU_ALARM_HUMID:
        case MENU_ALARM_MOTOR:
            return MENU_ALARM;
            
        case MENU_ALARM_TEMP_LOW:
        case MENU_ALARM_TEMP_HIGH:
            return MENU_ALARM_TEMP;
            
        case MENU_ALARM_HUMID_LOW:
        case MENU_ALARM_HUMID_HIGH:
            return MENU_ALARM_HUMID;
            
        case MENU_MANUAL_INCUBATION:
        case MENU_MANUAL_DEV_TEMP:
        case MENU_MANUAL_HATCH_TEMP:
        case MENU_MANUAL_DEV_HUMID:
        case MENU_MANUAL_HATCH_HUMID:
        case MENU_MANUAL_DEV_DAYS:
        case MENU_MANUAL_HATCH_DAYS:
            return MENU_INCUBATION_TYPE;
            
        default:
            return MENU_MAIN;
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
        // Terminal menü durumları için boş vector döndür (bunlar değer ayarlama ekranlarıdır)
        case MENU_TEMPERATURE:
        case MENU_HUMIDITY:
        case MENU_PID_KP:
        case MENU_PID_KI:
        case MENU_PID_KD:
        case MENU_PID_AUTO_TUNE:
        case MENU_MOTOR_WAIT:
        case MENU_MOTOR_RUN:
        case MENU_SET_TIME:
        case MENU_SET_DATE:
        case MENU_CALIBRATION_TEMP:
        case MENU_CALIBRATION_HUMID:
        case MENU_ALARM_TEMP_LOW:
        case MENU_ALARM_TEMP_HIGH:
        case MENU_ALARM_HUMID_LOW:
        case MENU_ALARM_HUMID_HIGH:
        case MENU_ALARM_MOTOR:
        case MENU_MANUAL_DEV_TEMP:
        case MENU_MANUAL_HATCH_TEMP:
        case MENU_MANUAL_DEV_HUMID:
        case MENU_MANUAL_HATCH_HUMID:
        case MENU_MANUAL_DEV_DAYS:
        case MENU_MANUAL_HATCH_DAYS:
        case MENU_ADJUST_VALUE:
        case MENU_WIFI_SETTINGS:
        case MENU_NONE:
            return std::vector<MenuItem>(); // Boş liste
        default:
            return std::vector<MenuItem>(); // Bilinmeyen durumlar için boş liste
    }
}