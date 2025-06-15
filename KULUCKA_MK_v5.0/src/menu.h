/**
 * @file menu.h
 * @brief Menü yönetimi
 * @version 1.4 - PID menü sistemi iyileştirildi
 */

#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "joystick.h"

// Menü durumları
enum MenuState {
    MENU_NONE,              // Menüde değil
    MENU_MAIN,              // Ana menü
    MENU_INCUBATION_TYPE,   // Kuluçka tipleri alt menüsü
    MENU_TEMPERATURE,       // Sıcaklık ayarları alt menüsü
    MENU_HUMIDITY,          // Nem ayarları alt menüsü
    MENU_PID,               // PID ayarları alt menüsü
    MENU_PID_KP,            // PID Kp ayarı
    MENU_PID_KI,            // PID Ki ayarı
    MENU_PID_KD,            // PID Kd ayarı
    MENU_PID_MODE,          // PID modu ana menüsü (YENİDEN YAPILANDIRILDI)
    MENU_PID_STATUS,        // YENİ: PID durum görüntüleme
    MENU_PID_AUTO_TUNE,     // PID otomatik ayarlama ekranı
    MENU_PID_MANUAL_START,  // Manuel PID başlatma ekranı
    MENU_PID_OFF,           // PID kapatma ekranı
    MENU_PID_MODE_MANUAL,   // YENİ: Manuel mod seçimi
    MENU_PID_MODE_AUTO,     // YENİ: Otomatik mod seçimi
    MENU_MOTOR,             // Motor ayarları alt menüsü
    MENU_MOTOR_WAIT,        // Motor bekleme süresi ayar ekranı
    MENU_MOTOR_RUN,         // Motor çalışma süresi ayar ekranı
    MENU_TIME_DATE,         // Saat ve tarih ayarları alt menüsü
    MENU_SET_TIME,          // Saat ayar ekranı
    MENU_SET_DATE,          // Tarih ayar ekranı
    MENU_CALIBRATION,       // Kalibrasyon ayarları alt menüsü
    MENU_CALIBRATION_TEMP,  // Sıcaklık kalibrasyon alt menüsü
    MENU_CALIBRATION_TEMP_1,// Sensör 1 sıcaklık kalibrasyon ekranı
    MENU_CALIBRATION_TEMP_2,// Sensör 2 sıcaklık kalibrasyon ekranı
    MENU_CALIBRATION_HUMID, // Nem kalibrasyon alt menüsü
    MENU_CALIBRATION_HUMID_1,// Sensör 1 nem kalibrasyon ekranı
    MENU_CALIBRATION_HUMID_2,// Sensör 2 nem kalibrasyon ekranı
    MENU_ALARM,             // Alarm ayarları alt menüsü
    MENU_ALARM_ENABLE_ALL,  // Tüm alarmları aç
    MENU_ALARM_DISABLE_ALL, // Tüm alarmları kapat
    MENU_ALARM_TEMP,        // Sıcaklık alarm ayarları alt menüsü
    MENU_ALARM_TEMP_LOW,    // Düşük sıcaklık alarm ekranı
    MENU_ALARM_TEMP_HIGH,   // Yüksek sıcaklık alarm ekranı
    MENU_ALARM_HUMID,       // Nem alarm ayarları alt menüsü
    MENU_ALARM_HUMID_LOW,   // Düşük nem alarm ekranı
    MENU_ALARM_HUMID_HIGH,  // Yüksek nem alarm ekranı
    MENU_ALARM_MOTOR,       // Motor alarm ayarları alt menüsü
    MENU_SENSOR_VALUES,     // Sensör değerleri görüntüleme
    MENU_MANUAL_INCUBATION, // Manuel kuluçka ayarları alt menüsü
    MENU_MANUAL_DEV_TEMP,   // Manuel gelişim sıcaklığı ekranı
    MENU_MANUAL_HATCH_TEMP, // Manuel çıkım sıcaklığı ekranı
    MENU_MANUAL_DEV_HUMID,  // Manuel gelişim nemi ekranı
    MENU_MANUAL_HATCH_HUMID,// Manuel çıkım nemi ekranı
    MENU_MANUAL_DEV_DAYS,   // Manuel gelişim günleri ekranı
    MENU_MANUAL_HATCH_DAYS, // Manuel çıkım günleri ekranı
    MENU_MANUAL_START,      // Manuel kuluçka başlatma ekranı
    MENU_ADJUST_VALUE,      // Değer ayarlama ekranı
    MENU_WIFI_SETTINGS,     // WiFi ayarları ekranı
    MENU_WIFI_MODE,         // WiFi modu seçimi (AP/Station)
    MENU_WIFI_SSID,         // WiFi SSID ayarlama
    MENU_WIFI_PASSWORD,     // WiFi Password ayarlama
    MENU_WIFI_CONNECT       // WiFi bağlantı ekranı
};

// Menü öğesi yapısı
struct MenuItem {
    String name;           // Menü öğesi adı
    MenuState nextState;   // Bu öğeye tıklayınca geçilecek menü durumu
};

class MenuManager {
public:
    // Yapılandırıcı
    MenuManager();
    
    // Menü yönetimini başlat
    bool begin();
    
    // Menüyü güncelle
    void update(JoystickDirection direction);
    
    // Mevcut menü durumunu al
    MenuState getCurrentState() const;
    
    // Önceki menü durumunu al
    MenuState getPreviousState() const;
    
    // Ana ekrana geri dön
    void returnToHome();
    
    // Değer ayarlama ekranını göster
    void showValueAdjustScreen(String title, float value, String unit, float minValue, float maxValue, float step);

    // Saat ayarlama ekranını göster
    void showTimeAdjustScreen(String title, int timeValue);
    
    // Tarih ayarlama ekranını göster
    void showDateAdjustScreen(String title, long dateValue);

    // Mevcut menü durumunu ayarla
    void setCurrentState(MenuState state);

    // Seçili indeksi ayarla
    void setSelectedIndex(int index);
    
    // Ayarlanan değeri al
    float getAdjustedValue() const;
    
    // Ayarlanan saat değerini al
    int getAdjustedTimeValue() const;
    
    // Ayarlanan tarih değerini al
    long getAdjustedDateValue() const;
    
    // Değer ayarlama ekranı başlığını al
    String getAdjustTitle() const;
    
    // Değer ayarlama ekranı birimini al
    String getAdjustUnit() const;
    
    // Saat string formatını al (HH:MM)
    String getTimeString() const;
    
    // Tarih string formatını al (DD.MM.YYYY)
    String getDateString() const;
    
    // Saat alanı indeksini al (0=saat, 1=dakika)
    int getTimeField() const;
    
    // Tarih alanı indeksini al (0=gün, 1=ay, 2=yıl)
    int getDateField() const;
    
    // Onay mesajı göster
    void showConfirmation(String message);
    
    // Menü listesini al
    std::vector<String> getMenuItems() const;
    
    // Seçili menü öğesi indeksini al
    int getSelectedIndex() const;
    
    // Menü kaydırma offset değerini al
    int getMenuOffset() const;
    
    // Menü seçimi yap
    bool selectMenuItem(int index);
    
    // Bir üst menüye dön
    bool goBack();
    
    // Ana ekranda mı kontrolü
    bool isInHomeScreen() const;
    
    // Menüde mi kontrolü
    bool isInMenu() const;
    
    // Değer ayarlama ekranında mı kontrolü
    bool isInValueAdjustScreen() const;
    
    // Saat ayarlama ekranında mı kontrolü
    bool isInTimeAdjustScreen() const;
    
    // Tarih ayarlama ekranında mı kontrolü
    bool isInDateAdjustScreen() const;
    
    // Kullanıcı etkileşimi son ne zaman oldu?
    unsigned long getLastInteractionTime() const;
    
    // Zamanı güncelle
    void updateInteractionTime();
    
    // PID durumu için menü öğelerini güncelle (YENİDEN YAPILANDIRILDI)
    void updatePIDMenuItems();
    
    // WiFi durumu için menü öğelerini güncelle
    void updateWiFiMenuItems();

    // Alarm durumu için menü öğelerini güncelle 
    void updateAlarmMenuItems();    

private:
    // Menü durumu
    MenuState _currentState;
    
    // Önceki menü durumu
    MenuState _previousState;
    
    // Seçili menü öğesi indeksi
    int _selectedIndex;
    
    // Menü kaydırma offset'i
    int _menuOffset;
    
    // Ana menü öğeleri
    std::vector<MenuItem> _mainMenuItems;
    
    // Kuluçka tipleri alt menüsü öğeleri
    std::vector<MenuItem> _incubationTypeItems;
    
    // Saat ve tarih alt menüsü öğeleri
    std::vector<MenuItem> _timeDateItems;
    
    // Kalibrasyon alt menüsü öğeleri
    std::vector<MenuItem> _calibrationItems;
    
    // Sıcaklık kalibrasyon alt menüsü öğeleri
    std::vector<MenuItem> _tempCalibrationItems;
    
    // Nem kalibrasyon alt menüsü öğeleri
    std::vector<MenuItem> _humidCalibrationItems;
    
    // Alarm alt menüsü öğeleri
    std::vector<MenuItem> _alarmItems;
    
    // Sıcaklık alarm alt menüsü öğeleri
    std::vector<MenuItem> _tempAlarmItems;
    
    // Nem alarm alt menüsü öğeleri
    std::vector<MenuItem> _humidAlarmItems;
    
    // Motor alt menüsü öğeleri
    std::vector<MenuItem> _motorItems;
    
    // Manuel kuluçka alt menüsü öğeleri
    std::vector<MenuItem> _manualIncubationItems;
    
    // PID ana menü öğeleri (YENİDEN YAPILANDIRILDI)
    std::vector<MenuItem> _pidItems;
    
    // PID manuel ayar alt menüsü öğeleri
    std::vector<MenuItem> _pidManualItems;
    
    // WiFi ayarları alt menüsü öğeleri
    std::vector<MenuItem> _wifiItems;
    
    // Değer ayarlama
    float _adjustValue;
    float _minValue;
    float _maxValue;
    float _stepValue;
    String _adjustTitle;
    String _adjustUnit;
    
    // Saat ve tarih ayarlama
    int _timeValue;      // HHMM formatında
    long _dateValue;     // DDMMYYYY formatında  
    int _timeField;      // 0=saat, 1=dakika
    int _dateField;      // 0=gün, 1=ay, 2=yıl
    
    // Menü değişimi kontrolü
    bool _menuChanged;
    
    // Son kullanıcı etkileşim zamanı
    unsigned long _lastInteractionTime;
    
    // Menü bölmelerini başlat
    void _initializeMenuItems();
    
    // Mevcut durum için menü öğelerini al
    std::vector<MenuItem> _getCurrentMenuItems() const;

    // Geri dönüş durumunu belirle
    MenuState _getBackState(MenuState currentState);

    // Terminal menü kontrolü
    bool _isTerminalMenu(MenuState state) const;
    
    // Menü kaydırma offset'ini güncelle
    void _updateMenuOffset();
    
    // Saat ayarlama için özel menü işlemleri
    void _handleTimeAdjustment(JoystickDirection direction);
    
    // Tarih ayarlama için özel menü işlemleri  
    void _handleDateAdjustment(JoystickDirection direction);
    
    // Saat değerini güvenli aralıkta tut
    void _validateTimeValue();
    
    // Tarih değerini güvenli aralıkta tut
    void _validateDateValue();
};

#endif // MENU_H