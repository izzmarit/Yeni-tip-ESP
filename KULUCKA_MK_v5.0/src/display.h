/**
 * @file display.h
 * @brief TFT LCD ekran yönetimi
 * @version 1.2
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "config.h"

// Ekran modları
enum DisplayMode {
    DISPLAY_NONE,         // Başlangıç durumu
    DISPLAY_SPLASH,       // Açılış ekranı
    DISPLAY_MAIN,         // Ana ekran
    DISPLAY_MENU,         // Menü ekranı
    DISPLAY_SUBMENU,      // Alt menü ekranı
    DISPLAY_VALUE_ADJUST, // Değer ayarlama ekranı
    DISPLAY_TIME_ADJUST,  // Saat ayarlama ekranı
    DISPLAY_DATE_ADJUST,  // Tarih ayarlama ekranı
    DISPLAY_CONFIRMATION, // Onay mesajı ekranı
    DISPLAY_ALARM,        // Alarm mesajı ekranı
    DISPLAY_PID_STATUS    // PID durum ekranı
};

class Display {
public:
    // Yapılandırıcı
    Display();
    
    // Ekranı başlat
    bool begin();
    
    // Açılış ekranını göster
    void showSplashScreen();
    
    // Ana ekranı oluştur
    void setupMainScreen();
    
    // Ekranı tamamen temizle
    void clear();
    
    // Ana ekranı güncelle
    void updateMainScreen(float currentTemp, float targetTemp, float currentHumid, 
                          float targetHumid, int motorMinutesLeft, int motorSecondsLeft, 
                          int currentDay, int totalDays, String incubationType,
                          bool heatingActive, bool humidActive, bool motorActive,
                          String timeStr, String dateStr);
    
    // Menü ekranını göster
    void showMenu(String menuItems[], int itemCount, int selectedItem);
    
    // Alt menü ekranını göster
    void showSubmenu(String submenuItems[], int itemCount, int selectedItem);
    
    // Değer ayarlama ekranını göster (String değer için)
    void showValueAdjustScreen(String title, String value, String unit);
    
    // Değer ayarlama ekranını göster (float değer için)
    void showValueAdjustScreen(String title, float value, String unit);
    
    // Saat ayarlama ekranını göster
    void showTimeAdjustScreen(String title, String timeString, int selectedField);
    
    // Tarih ayarlama ekranını göster
    void showDateAdjustScreen(String title, String dateString, int selectedField);
    
    // Sensör değerleri ekranını göster
    void showSensorValuesScreen(float temp1, float humid1, float temp2, float humid2, 
                               bool sensor1Working, bool sensor2Working);
    
    // Onay mesajı göster
    void showConfirmationMessage(String message);
    
    // Alarm mesajı göster
    void showAlarmMessage(String alarmType, String alarmValue);
    
    // PID durum ekranını göster
    void showPIDStatusScreen(String pidMode, String pidValues);
    
    // İlerleme çubuğu göster
    void showProgressBar(int x, int y, int width, int height, uint16_t color, int percentage);
    
    // Menü öğelerinin değiştiğini belirt
    void setMenuChanged();
    
    // Mevcut ekran modunu al
    DisplayMode getCurrentMode();

private:
    Adafruit_ST7735 _tft;
    
    // Mevcut ekran modu
    DisplayMode _currentMode;
    
    // Menü durumu
    int _lastSelectedItem;
    bool _menuChanged;
    
    // Son değerleri saklayan değişkenler
    float _lastTemp = -999;
    float _lastTargetTemp = -999;
    float _lastHumid = -999;
    float _lastTargetHumid = -999;
    int _lastMotorMin = -1;
    int _lastMotorSec = -1;
    int _lastDay = -1;
    int _lastTotalDays = -1;
    bool _lastHeatingActive = false;
    bool _lastHumidActive = false;
    bool _lastMotorActive = false;
    String _lastTimeStr = "";
    String _lastDateStr = "";
    String _lastType = "";
    
    // Tam ekran yenileme gerekli mi?
    bool _needsFullRedraw = true;
    
    // Ekranı bölmelere ayır
    void _drawDividers();
    
    // Bilgi satırını güncelle
    void _updateInfoBar(String timeStr, String dateStr);
    
    // Sıcaklık bölmesini güncelle
    void _updateTempSection(float currentTemp, float targetTemp, bool heatingActive);
    
    // Nem bölmesini güncelle
    void _updateHumidSection(float currentHumid, float targetHumid, bool humidActive);
    
    // Motor bölmesini güncelle
    void _updateMotorSection(int minutesLeft, int secondsLeft, bool motorActive);
    
    // Kuluçka bölmesini güncelle
    void _updateIncubationSection(int currentDay, int totalDays, String incubationType);
};

#endif // DISPLAY_H