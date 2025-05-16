/**
 * @file display.h
 * @brief TFT LCD ekran yönetimi
 * @version 1.0
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "config.h"

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
    
    // Onay mesajı göster
    void showConfirmationMessage(String message);
    
    // Alarm mesajı göster
    void showAlarmMessage(String alarmType, String alarmValue);
    
    // İlerleme çubuğu göster
    void showProgressBar(int x, int y, int width, int height, uint16_t color, int percentage);

private:
    Adafruit_ST7735 _tft;
    
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
    
    // Yanıp sönen metin göster
    void _blinkText(int x, int y, String text, uint16_t color);
};

#endif // DISPLAY_H