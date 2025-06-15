/**
 * @file menu.cpp
 * @brief Menü yönetimi uygulaması
 * @version 1.6 - PID menü sistemi iyileştirildi, kuluçka tipleri düzeltildi
 */

#include "menu.h"
#include "alarm.h"  // AlarmManager için gerekli
#include "pid.h"    // PIDController ve PIDMode için gerekli

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
    _timeValue = 0;
    _dateValue = 0;
    _timeField = 0;
    _dateField = 0;
    _lastInteractionTime = 0;
    _menuChanged = true;
    _menuOffset = 0;
    
    // Menü öğelerini başlat
    _initializeMenuItems();
}

bool MenuManager::begin() {
    return true;
}

void MenuManager::_initializeMenuItems() {
    // Ana menü öğeleri
    _mainMenuItems.clear();
    _mainMenuItems.push_back({"Kulucka Tipleri", MENU_INCUBATION_TYPE});
    _mainMenuItems.push_back({"Sicaklik", MENU_TEMPERATURE});
    _mainMenuItems.push_back({"Nem", MENU_HUMIDITY});
    _mainMenuItems.push_back({"PID Modu", MENU_PID_MODE});
    _mainMenuItems.push_back({"Motor", MENU_MOTOR});
    _mainMenuItems.push_back({"Saat ve Tarih", MENU_TIME_DATE});
    _mainMenuItems.push_back({"Kalibrasyon", MENU_CALIBRATION});
    _mainMenuItems.push_back({"Alarm", MENU_ALARM});
    _mainMenuItems.push_back({"Sensor Degerleri", MENU_SENSOR_VALUES});
    _mainMenuItems.push_back({"WiFi Ayarlari", MENU_WIFI_SETTINGS});
    
    // Kuluçka tipleri alt menüsü öğeleri - SADECE 4 TİP
    _incubationTypeItems.clear();
    _incubationTypeItems.push_back({"Tavuk", MENU_NONE});
    _incubationTypeItems.push_back({"Bildircin", MENU_NONE});
    _incubationTypeItems.push_back({"Kaz", MENU_NONE});
    _incubationTypeItems.push_back({"Manuel", MENU_MANUAL_INCUBATION});
    
    // PID ayarları alt menüsü öğeleri (dinamik olarak güncellenecek)
    updatePIDMenuItems();
    
    // PID manuel ayar alt menüsü öğeleri
    _pidManualItems.clear();
    _pidManualItems.push_back({"PID Kp", MENU_PID_KP});
    _pidManualItems.push_back({"PID Ki", MENU_PID_KI});
    _pidManualItems.push_back({"PID Kd", MENU_PID_KD});
    _pidManualItems.push_back({"Manuel PID Baslat", MENU_PID_MANUAL_START});
    
    // Motor alt menüsü öğeleri
    _motorItems.clear();
    _motorItems.push_back({"Bekleme Suresi", MENU_MOTOR_WAIT});
    _motorItems.push_back({"Calisma Suresi", MENU_MOTOR_RUN});
    
    // Saat ve tarih alt menüsü öğeleri
    _timeDateItems.clear();
    _timeDateItems.push_back({"Saati ayarla", MENU_SET_TIME});
    _timeDateItems.push_back({"Tarihi ayarla", MENU_SET_DATE});
    
    // Kalibrasyon alt menüsü öğeleri
    _calibrationItems.clear();
    _calibrationItems.push_back({"Sicaklik Kalibrasyon", MENU_CALIBRATION_TEMP});
    _calibrationItems.push_back({"Nem Kalibrasyon", MENU_CALIBRATION_HUMID});
    
    // Sıcaklık kalibrasyon alt menüsü öğeleri
    _tempCalibrationItems.clear();
    _tempCalibrationItems.push_back({"Sensor 1 Sicaklik", MENU_CALIBRATION_TEMP_1});
    _tempCalibrationItems.push_back({"Sensor 2 Sicaklik", MENU_CALIBRATION_TEMP_2});
    
    // Nem kalibrasyon alt menüsü öğeleri
    _humidCalibrationItems.clear();
    _humidCalibrationItems.push_back({"Sensor 1 Nem", MENU_CALIBRATION_HUMID_1});
    _humidCalibrationItems.push_back({"Sensor 2 Nem", MENU_CALIBRATION_HUMID_2});
    
    // Alarm alt menüsü öğeleri (dinamik olarak güncellenecek)
    updateAlarmMenuItems();
    
    // Sıcaklık alarm alt menüsü öğeleri
    _tempAlarmItems.clear();
    _tempAlarmItems.push_back({"Dusuk Sicaklik", MENU_ALARM_TEMP_LOW});
    _tempAlarmItems.push_back({"Yuksek Sicaklik", MENU_ALARM_TEMP_HIGH});
    
    // Nem alarm alt menüsü öğeleri
    _humidAlarmItems.clear();
    _humidAlarmItems.push_back({"Dusuk Nem", MENU_ALARM_HUMID_LOW});
    _humidAlarmItems.push_back({"Yuksek Nem", MENU_ALARM_HUMID_HIGH});
    
    // Manuel kuluçka alt menüsü öğeleri
    _manualIncubationItems.clear();
    _manualIncubationItems.push_back({"Gelisim Sicakligi", MENU_MANUAL_DEV_TEMP});
    _manualIncubationItems.push_back({"Cikim Sicakligi", MENU_MANUAL_HATCH_TEMP});
    _manualIncubationItems.push_back({"Gelisim Nemi", MENU_MANUAL_DEV_HUMID});
    _manualIncubationItems.push_back({"Cikim Nemi", MENU_MANUAL_HATCH_HUMID});
    _manualIncubationItems.push_back({"Gelisim Gunleri", MENU_MANUAL_DEV_DAYS});
    _manualIncubationItems.push_back({"Cikim Gunleri", MENU_MANUAL_HATCH_DAYS});
    _manualIncubationItems.push_back({"Manuel Baslat", MENU_MANUAL_START});
    
    // WiFi ayarları alt menüsü öğeleri (dinamik olarak güncellenecek)
    updateWiFiMenuItems();
}

void MenuManager::updatePIDMenuItems() {
    // External PID controller referansını al
    extern PIDController pidController;
    
    _pidItems.clear();
    
    // Mevcut PID modunu göster
    String currentModeStr = pidController.getPIDModeString();
    _pidItems.push_back({"Mevcut Mod: " + currentModeStr, MENU_NONE});
    
    // PID mod değiştirme seçenekleri
    if (pidController.getPIDMode() != PID_MODE_MANUAL) {
        _pidItems.push_back({"Manuel PID Baslat", MENU_PID_MANUAL_START});
    }
    
    if (pidController.getPIDMode() != PID_MODE_AUTO_TUNE) {
        _pidItems.push_back({"Otomatik Ayarlama", MENU_PID_AUTO_TUNE});
    }
    
    if (pidController.getPIDMode() != PID_MODE_OFF) {
        _pidItems.push_back({"PID'i Kapat", MENU_PID_OFF});
    }
    
    // PID parametreleri menüsü (sadece manuel modda veya kapalı modda)
    if (pidController.getPIDMode() == PID_MODE_MANUAL || pidController.getPIDMode() == PID_MODE_OFF) {
        _pidItems.push_back({"PID Parametreleri", MENU_PID});
    }
    
    _menuChanged = true;
}

void MenuManager::updateWiFiMenuItems() {
    _wifiItems.clear();
    _wifiItems.push_back({"WiFi Modu", MENU_WIFI_MODE});
    _wifiItems.push_back({"Ag Adi (SSID)", MENU_WIFI_SSID});
    _wifiItems.push_back({"Sifre", MENU_WIFI_PASSWORD});
    _wifiItems.push_back({"Baglan", MENU_WIFI_CONNECT});
}

void MenuManager::updateAlarmMenuItems() {
    // External referanslar ile alarm durumunu al
    extern AlarmManager alarmManager;
    
    // Alarm ana menü öğelerini dinamik olarak güncelle
    bool alarmsEnabled = alarmManager.areAlarmsEnabled();
    String alarmToggleText = alarmsEnabled ? "Tum Alarmlari Kapat" : "Tum Alarmlari Ac";
    MenuState alarmToggleState = alarmsEnabled ? MENU_ALARM_DISABLE_ALL : MENU_ALARM_ENABLE_ALL;
    
    _alarmItems.clear();
    _alarmItems.push_back({alarmToggleText, alarmToggleState});
    _alarmItems.push_back({"Sicaklik Alarmlari", MENU_ALARM_TEMP});
    _alarmItems.push_back({"Nem Alarmlari", MENU_ALARM_HUMID});
    _alarmItems.push_back({"Motor Alarmlari", MENU_ALARM_MOTOR});
    
    _menuChanged = true; // Menünün yeniden çizilmesini sağla
    
    Serial.println("Alarm menü öğeleri güncellendi. Mevcut durum: " + String(alarmsEnabled ? "AÇIK" : "KAPALI"));
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
    
    // Saat ayarlama ekranında
    if (isInTimeAdjustScreen()) {
        _handleTimeAdjustment(direction);
        return;
    }
    
    // Tarih ayarlama ekranında
    if (isInDateAdjustScreen()) {
        _handleDateAdjustment(direction);
        return;
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
            _menuOffset = 0;
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
            _menuOffset = 0;
            _menuChanged = true;
        }
        return;
    }
    
    switch (direction) {
        case JOYSTICK_UP:
            _selectedIndex = (_selectedIndex > 0) ? _selectedIndex - 1 : currentItems.size() - 1;
            _updateMenuOffset();
            _menuChanged = true;
            break;
            
        case JOYSTICK_DOWN:
            _selectedIndex = (_selectedIndex < currentItems.size() - 1) ? _selectedIndex + 1 : 0;
            _updateMenuOffset();
            _menuChanged = true;
            break;
            
        case JOYSTICK_RIGHT:
            if (_selectedIndex < currentItems.size()) {
                MenuState nextState = currentItems[_selectedIndex].nextState;
                if (nextState != MENU_NONE) {
                    _previousState = _currentState;
                    _currentState = nextState;
                    _selectedIndex = 0;
                    _menuOffset = 0;
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
                    _selectedIndex = 0;
                    _menuOffset = 0;
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
                _menuOffset = 0;
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

void MenuManager::_updateMenuOffset() {
    const int maxVisibleItems = 6; // Ekranda maksimum görünebilir öğe sayısı
    
    // Seçili öğe görünür alanın üstündeyse yukarı kaydır
    if (_selectedIndex < _menuOffset) {
        _menuOffset = _selectedIndex;
    }
    // Seçili öğe görünür alanın altındaysa aşağı kaydır
    else if (_selectedIndex >= _menuOffset + maxVisibleItems) {
        _menuOffset = _selectedIndex - maxVisibleItems + 1;
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
        case MENU_PID_MANUAL_START:
        case MENU_PID_OFF:
        case MENU_MOTOR_WAIT:
        case MENU_MOTOR_RUN:
        case MENU_SET_TIME:
        case MENU_SET_DATE:
        case MENU_CALIBRATION_TEMP_1:
        case MENU_CALIBRATION_TEMP_2:
        case MENU_CALIBRATION_HUMID_1:
        case MENU_CALIBRATION_HUMID_2:
        case MENU_ALARM_ENABLE_ALL:    // YENİ EKLENDİ
        case MENU_ALARM_DISABLE_ALL:   // YENİ EKLENDİ
        case MENU_ALARM_TEMP_LOW:
        case MENU_ALARM_TEMP_HIGH:
        case MENU_ALARM_HUMID_LOW:
        case MENU_ALARM_HUMID_HIGH:
        case MENU_ALARM_MOTOR:
        case MENU_SENSOR_VALUES:
        case MENU_MANUAL_DEV_TEMP:
        case MENU_MANUAL_HATCH_TEMP:
        case MENU_MANUAL_DEV_HUMID:
        case MENU_MANUAL_HATCH_HUMID:
        case MENU_MANUAL_DEV_DAYS:
        case MENU_MANUAL_HATCH_DAYS:
        case MENU_MANUAL_START:
        case MENU_WIFI_MODE:
        case MENU_WIFI_SSID:
        case MENU_WIFI_PASSWORD:
        case MENU_WIFI_CONNECT:
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
        case MENU_SENSOR_VALUES:
        case MENU_WIFI_SETTINGS:
            return MENU_MAIN;
            
        case MENU_PID:
        case MENU_PID_AUTO_TUNE:
        case MENU_PID_MANUAL_START:
        case MENU_PID_OFF:
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
            
        case MENU_CALIBRATION_TEMP_1:
        case MENU_CALIBRATION_TEMP_2:
            return MENU_CALIBRATION_TEMP;
            
        case MENU_CALIBRATION_HUMID_1:
        case MENU_CALIBRATION_HUMID_2:
            return MENU_CALIBRATION_HUMID;
            
        case MENU_ALARM_ENABLE_ALL:    // YENİ EKLENDİ
        case MENU_ALARM_DISABLE_ALL:   // YENİ EKLENDİ
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
        case MENU_MANUAL_START:
            return MENU_INCUBATION_TYPE;
            
        case MENU_WIFI_MODE:
        case MENU_WIFI_SSID:
        case MENU_WIFI_PASSWORD:
        case MENU_WIFI_CONNECT:
            return MENU_WIFI_SETTINGS;
            
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
    _menuOffset = 0;
    _menuChanged = true;
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
    _menuChanged = true;
}

void MenuManager::showTimeAdjustScreen(String title, int timeValue) {
    _previousState = _currentState;
    _currentState = MENU_SET_TIME;
    _timeValue = timeValue;
    _timeField = 0; // Başlangıçta saat alanı seçili
    _adjustTitle = title;
    _menuChanged = true;
    _validateTimeValue();
}

void MenuManager::showDateAdjustScreen(String title, long dateValue) {
    _previousState = _currentState;
    _currentState = MENU_SET_DATE;
    _dateValue = dateValue;
    _dateField = 0; // Başlangıçta gün alanı seçili
    _adjustTitle = title;
    _menuChanged = true;
    _validateDateValue();
}

float MenuManager::getAdjustedValue() const {
    return _adjustValue;
}

int MenuManager::getAdjustedTimeValue() const {
    return _timeValue;
}

long MenuManager::getAdjustedDateValue() const {
    return _dateValue;
}

String MenuManager::getAdjustTitle() const {
    return _adjustTitle;
}

String MenuManager::getAdjustUnit() const {
    return _adjustUnit;
}

String MenuManager::getTimeString() const {
    int hour = _timeValue / 100;
    int minute = _timeValue % 100;
    
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", hour, minute);
    
    return String(timeStr);
}

String MenuManager::getDateString() const {
    int day = (int)(_dateValue / 1000000);
    int month = (int)((_dateValue / 10000) % 100);
    int year = (int)(_dateValue % 10000);
    
    char dateStr[11];
    sprintf(dateStr, "%02d.%02d.%04d", day, month, year);
    
    return String(dateStr);
}

int MenuManager::getTimeField() const {
    return _timeField;
}

int MenuManager::getDateField() const {
    return _dateField;
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

int MenuManager::getMenuOffset() const {
    return _menuOffset;
}

bool MenuManager::selectMenuItem(int index) {
    std::vector<MenuItem> currentItems = _getCurrentMenuItems();
    
    if (index >= 0 && index < currentItems.size()) {
        _selectedIndex = index;
        _updateMenuOffset();
        _menuChanged = true;
        return true;
    }
    
    return false;
}

bool MenuManager::goBack() {
    if (_currentState == MENU_MAIN) {
        // Ana menüden ana ekrana dön
        _currentState = MENU_NONE;
        _selectedIndex = 0;
        _menuOffset = 0;
        _menuChanged = true;
        return true;
    } else if (_currentState != MENU_NONE) {
        // Diğer menülerden ana menüye dön
        _currentState = MENU_MAIN;
        _selectedIndex = 0;
        _menuOffset = 0;
        _menuChanged = true;
        return true;
    }
    
    return false;
}

bool MenuManager::isInHomeScreen() const {
    return _currentState == MENU_NONE;
}

bool MenuManager::isInMenu() const {
    return _currentState != MENU_NONE && _currentState != MENU_ADJUST_VALUE && 
           !isInTimeAdjustScreen() && !isInDateAdjustScreen();
}

bool MenuManager::isInValueAdjustScreen() const {
    return _currentState == MENU_ADJUST_VALUE;
}

bool MenuManager::isInTimeAdjustScreen() const {
    return _currentState == MENU_SET_TIME && _previousState != MENU_TIME_DATE;
}

bool MenuManager::isInDateAdjustScreen() const {
    return _currentState == MENU_SET_DATE && _previousState != MENU_TIME_DATE;
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
    _selectedIndex = 0;
    _menuOffset = 0;
    _menuChanged = true;
    updateInteractionTime();
}

void MenuManager::setSelectedIndex(int index) {
    // İndeksin geçerli aralıkta olduğundan emin ol
    std::vector<MenuItem> currentItems = _getCurrentMenuItems();
    
    if (!currentItems.empty() && index >= 0 && index < currentItems.size()) {
        _selectedIndex = index;
        _updateMenuOffset();
        _menuChanged = true;
        updateInteractionTime();
    } else if (!currentItems.empty()) {
        _selectedIndex = 0;
        _menuOffset = 0;
        _menuChanged = true;
    }
}

void MenuManager::_handleTimeAdjustment(JoystickDirection direction) {
    switch (direction) {
        case JOYSTICK_UP:
            if (_timeField == 0) { // Saat alanı
                int hour = _timeValue / 100;
                hour = (hour + 1) % 24;
                _timeValue = hour * 100 + (_timeValue % 100);
            } else { // Dakika alanı
                int minute = _timeValue % 100;
                minute = (minute + 1) % 60;
                _timeValue = (_timeValue / 100) * 100 + minute;
            }
            _validateTimeValue();
            break;
            
        case JOYSTICK_DOWN:
            if (_timeField == 0) { // Saat alanı
                int hour = _timeValue / 100;
                hour = (hour - 1 + 24) % 24;
                _timeValue = hour * 100 + (_timeValue % 100);
            } else { // Dakika alanı
                int minute = _timeValue % 100;
                minute = (minute - 1 + 60) % 60;
                _timeValue = (_timeValue / 100) * 100 + minute;
            }
            _validateTimeValue();
            break;
            
        case JOYSTICK_LEFT:
            // Alan değiştir (saat <-> dakika)
            _timeField = (_timeField == 0) ? 1 : 0;
            break;
            
        case JOYSTICK_RIGHT:
            // Alan değiştir (saat <-> dakika)
            _timeField = (_timeField == 0) ? 1 : 0;
            break;
            
        case JOYSTICK_PRESS:
            // Değer kaydedilecek - main.cpp'de işlenecek
            break;
    }
}

void MenuManager::_handleDateAdjustment(JoystickDirection direction) {
    int day = (int)(_dateValue / 1000000);
    int month = (int)((_dateValue / 10000) % 100);
    int year = (int)(_dateValue % 10000);
    
    switch (direction) {
        case JOYSTICK_UP:
            if (_dateField == 0) { // Gün alanı
                day = (day % 31) + 1;
            } else if (_dateField == 1) { // Ay alanı
                month = (month % 12) + 1;
            } else { // Yıl alanı
                year = min(year + 1, 2050);
            }
            break;
            
        case JOYSTICK_DOWN:
            if (_dateField == 0) { // Gün alanı
                day = (day - 1);
                if (day < 1) day = 31;
            } else if (_dateField == 1) { // Ay alanı
                month = (month - 1);
                if (month < 1) month = 12;
            } else { // Yıl alanı
                year = max(year - 1, 2025);
            }
            break;
            
        case JOYSTICK_LEFT:
            // Alan değiştir (gün -> ay -> yıl -> gün)
            _dateField = (_dateField - 1 + 3) % 3;
            break;
            
        case JOYSTICK_RIGHT:
            // Alan değiştir (gün -> ay -> yıl -> gün)
            _dateField = (_dateField + 1) % 3;
            break;
            
        case JOYSTICK_PRESS:
            // Değer kaydedilecek - main.cpp'de işlenecek
            break;
    }
    
    _dateValue = day * 1000000L + month * 10000L + year;
    _validateDateValue();
}

void MenuManager::_validateTimeValue() {
    int hour = _timeValue / 100;
    int minute = _timeValue % 100;
    
    // Saat kontrolü (0-23)
    if (hour < 0) hour = 0;
    if (hour > 23) hour = 23;
    
    // Dakika kontrolü (0-59)
    if (minute < 0) minute = 0;
    if (minute > 59) minute = 59;
    
    _timeValue = hour * 100 + minute;
}

void MenuManager::_validateDateValue() {
    int day = (int)(_dateValue / 1000000);
    int month = (int)((_dateValue / 10000) % 100);
    int year = (int)(_dateValue % 10000);
    
    // Yıl kontrolü (2025-2050)
    if (year < 2025) year = 2025;
    if (year > 2050) year = 2050;
    
    // Ay kontrolü (1-12)
    if (month < 1) month = 1;
    if (month > 12) month = 12;
    
    // Gün kontrolü (1-31) - basit kontrol
    if (day < 1) day = 1;
    if (day > 31) day = 31;
    
    // Ayın günlerine göre daha detaylı kontrol
    if (month == 2) { // Şubat
        if (day > 29) day = 29; // Basitçe 29'a sınırla
    } else if (month == 4 || month == 6 || month == 9 || month == 11) { // 30 günlük aylar
        if (day > 30) day = 30;
    }
    
    _dateValue = day * 1000000L + month * 10000L + year;
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
        case MENU_CALIBRATION_TEMP:
            return _tempCalibrationItems;
        case MENU_CALIBRATION_HUMID:
            return _humidCalibrationItems;
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
        case MENU_WIFI_SETTINGS:
            return _wifiItems;
        // Terminal menü durumları için boş vector döndür
        default:
            return std::vector<MenuItem>(); // Boş liste
    }
}