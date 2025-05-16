/**
 * @file display.cpp
 * @brief TFT LCD ekran yönetimi uygulaması
 * @version 1.0
 */

#include "display.h"

Display::Display() : _tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST) {
    // Yapılandırıcı
}

bool Display::begin() {
    // SPI başlatma ve hız optimizasyonu
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    SPI.setFrequency(40000000); // 40 MHz
    
    // Ekran arka ışık pini kontrolü
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, HIGH);
    
    // Ekranı başlat
    _tft.initR(INITR_BLACKTAB);
    _tft.setRotation(1); // Rotasyon 3'ten 1'e değiştirildi
    _tft.fillScreen(COLOR_BACKGROUND);
    
    return true;
}

void Display::showSplashScreen() {
    _tft.fillScreen(COLOR_BACKGROUND);
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    // Üstteki "KULUÇKA" yazısı
    _tft.setTextSize(2);
    _tft.setTextColor(COLOR_TEXT);
    
    int16_t x1, y1;
    uint16_t w, h;
    
    // "KULUÇKA" yazısının boyutlarını ölç
    _tft.getTextBounds("KULUCKA", 0, 0, &x1, &y1, &w, &h);
    _tft.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT / 2 - h - 5);
    _tft.print("KULUCKA");
    
    // Alttaki "MK v5.0" yazısı
    _tft.getTextBounds("MK v5.0", 0, 0, &x1, &y1, &w, &h);
    _tft.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT / 2 + 5);
    _tft.print("MK v5.0");
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    delay(3000); // 3 saniye göster
    
    // Uzun beklemeden sonra watchdog besleme
    esp_task_wdt_reset();
}

void Display::setupMainScreen() {
    _tft.fillScreen(COLOR_BACKGROUND);
    _drawDividers();
    
    // Üst bilgi çubuğu (saat, isim, tarih)
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(35, 5);
    _tft.print("MK v5.0");
    
    // Sıcaklık bölmesi başlığı
    _tft.setCursor(16, 20);
    _tft.print("SICAKLIK");
    
    // Nem bölmesi başlığı
    _tft.setCursor(103, 20);
    _tft.print("NEM");
    
    // Motor bölmesi başlığı
    _tft.setCursor(19, 74);
    _tft.print("MOTOR");
    
    // Kuluçka bölmesi başlığı
    _tft.setCursor(95, 74);
    _tft.print("KULUCKA");
}

void Display::clear() {
    _tft.fillScreen(COLOR_BACKGROUND);
}

void Display::_drawDividers() {
    // Üst bilgi satırı bölücüsü
    _tft.drawFastHLine(0, 15, SCREEN_WIDTH, COLOR_DIVISION);
    
    // Dikey orta çizgi
    _tft.drawFastVLine(SCREEN_WIDTH / 2, 15, SCREEN_HEIGHT - 15, COLOR_DIVISION);
    
    // Yatay orta çizgi
    _tft.drawFastHLine(0, (SCREEN_HEIGHT - 15) / 2 + 15, SCREEN_WIDTH, COLOR_DIVISION);
}

void Display::_updateInfoBar(String timeStr, String dateStr) {
    // Saat, MK v5.0 metni, tarih bilgilerini güncelle
    _tft.fillRect(0, 0, SCREEN_WIDTH, 15, COLOR_BACKGROUND);
    
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    
    // Saat
    _tft.setCursor(2, 5);
    _tft.print(timeStr);
    
    // MK v5.0
    _tft.setCursor(60, 5);
    _tft.print("MK v5.0");
    
    // Tarih
    _tft.setCursor(105, 5);
    _tft.print(dateStr);
}

void Display::_updateTempSection(float currentTemp, float targetTemp, bool heatingActive) {
    // Sıcaklık bölmesini temizle
    _tft.fillRect(1, 16, SCREEN_WIDTH / 2 - 1, (SCREEN_HEIGHT - 15) / 2 - 1, COLOR_BACKGROUND);
    
    _tft.setTextSize(1);
    
    // SICAKLIK başlığı (yanıp sönme efekti için)
    if (heatingActive) {
        // millis() kullanarak yanıp sönme efekti
        if ((millis() / 500) % 2 == 0) {
            _tft.setTextColor(COLOR_TEMP);
        } else {
            _tft.setTextColor(COLOR_BACKGROUND);
        }
    } else {
        _tft.setTextColor(COLOR_TEXT);
    }
    
    _tft.setCursor(16, 20);
    _tft.print("SICAKLIK");
    
    // Hedef sıcaklık
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, 55);
    _tft.print("Hedef:");
    _tft.print(targetTemp, 1);
    _tft.print("C");
    _tft.write(247); // Derece sembolü
    
    // Mevcut sıcaklık
    _tft.setTextSize(2);
    _tft.setTextColor(COLOR_TEMP);
    
    char tempStr[7];
    sprintf(tempStr, "%4.1f", currentTemp);
    
    // Mevcut sıcaklığın boyutlarını ölç
    int16_t x1, y1;
    uint16_t w, h;
    _tft.getTextBounds(tempStr, 0, 0, &x1, &y1, &w, &h);
    _tft.setCursor((SCREEN_WIDTH / 2 - w) / 2, 35);
    _tft.print(tempStr);
    
    // Derece sembolü
    _tft.setCursor(_tft.getCursorX(), 35);
    _tft.print("C");
    _tft.write(247); // Derece sembolü
}

void Display::_updateHumidSection(float currentHumid, float targetHumid, bool humidActive) {
    // Nem bölmesini temizle
    _tft.fillRect(SCREEN_WIDTH / 2 + 1, 16, SCREEN_WIDTH / 2 - 1, (SCREEN_HEIGHT - 15) / 2 - 1, COLOR_BACKGROUND);
    
    _tft.setTextSize(1);
    
    // NEM başlığı (yanıp sönme efekti için)
    if (humidActive) {
        // millis() kullanarak yanıp sönme efekti
        if ((millis() / 500) % 2 == 0) {
            _tft.setTextColor(COLOR_HUMID);
        } else {
            _tft.setTextColor(COLOR_BACKGROUND);
        }
    } else {
        _tft.setTextColor(COLOR_TEXT);
    }
    
    _tft.setCursor(103, 20);
    _tft.print("NEM");
    
    // Hedef nem
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(85, 55);
    _tft.print("Hedef:%");
    _tft.print(targetHumid, 0);
    
    // Mevcut nem
    _tft.setTextSize(2);
    _tft.setTextColor(COLOR_HUMID);
    
    char humidStr[6];
    sprintf(humidStr, "%3.0f", currentHumid);
    
    // Mevcut nemin boyutlarını ölç
    int16_t x1, y1;
    uint16_t w, h;
    _tft.getTextBounds(humidStr, 0, 0, &x1, &y1, &w, &h);
    _tft.setCursor(SCREEN_WIDTH / 2 + (SCREEN_WIDTH / 2 - w) / 2, 35);
    _tft.print(humidStr);
    
    // Yüzde sembolü
    _tft.setCursor(_tft.getCursorX(), 35);
    _tft.print("%");
}

void Display::_updateMotorSection(int minutesLeft, int secondsLeft, bool motorActive) {
    // Motor bölmesini temizle
    _tft.fillRect(1, (SCREEN_HEIGHT - 15) / 2 + 16, SCREEN_WIDTH / 2 - 1, (SCREEN_HEIGHT - 15) / 2 - 1, COLOR_BACKGROUND);
    
    _tft.setTextSize(1);
    
    // MOTOR başlığı (yanıp sönme efekti için)
    if (motorActive) {
        // millis() kullanarak yanıp sönme efekti
        if ((millis() / 500) % 2 == 0) {
            _tft.setTextColor(COLOR_HIGHLIGHT);
        } else {
            _tft.setTextColor(COLOR_BACKGROUND);
        }
    } else {
        _tft.setTextColor(COLOR_TEXT);
    }
    
    _tft.setCursor(19, 74);
    _tft.print("MOTOR");
    
    _tft.setTextColor(COLOR_TEXT);
    
    // Dakika gösterimi
    _tft.setCursor(3, 90);
    _tft.print("Dk:");
    _tft.print(minutesLeft);
    
    // Saniye gösterimi
    _tft.setCursor(3, 105);
    _tft.print("Sn:");
    _tft.print(secondsLeft);
}

void Display::_updateIncubationSection(int currentDay, int totalDays, String incubationType) {
    // Kuluçka bölmesini temizle
    _tft.fillRect(SCREEN_WIDTH / 2 + 1, (SCREEN_HEIGHT - 15) / 2 + 16, SCREEN_WIDTH / 2 - 1, (SCREEN_HEIGHT - 15) / 2 - 1, COLOR_BACKGROUND);
    
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    
    // KULUCKA başlığı
    _tft.setCursor(95, 74);
    _tft.print("KULUCKA");
    
    // Kuluçka tipi
    _tft.setCursor(85, 105);
    _tft.print(incubationType);
    
    // Gün bilgisi (1/21 formatında)
    char dayStr[10];
    sprintf(dayStr, "%d/%d", currentDay, totalDays);
    
    _tft.setTextSize(2);
    _tft.setTextColor(COLOR_HIGHLIGHT);
    
    // Gün bilgisinin boyutlarını ölç
    int16_t x1, y1;
    uint16_t w, h;
    _tft.getTextBounds(dayStr, 0, 0, &x1, &y1, &w, &h);
    _tft.setCursor(SCREEN_WIDTH / 2 + (SCREEN_WIDTH / 2 - w) / 2, 90);
    _tft.print(dayStr);
}

void Display::updateMainScreen(float currentTemp, float targetTemp, float currentHumid, 
                               float targetHumid, int motorMinutesLeft, int motorSecondsLeft, 
                               int currentDay, int totalDays, String incubationType,
                               bool heatingActive, bool humidActive, bool motorActive,
                               String timeStr, String dateStr) {
    
    // Watchdog besleme - ekran güncellemesi başlangıcında
    esp_task_wdt_reset();
    
    _updateInfoBar(timeStr, dateStr);
    _updateTempSection(currentTemp, targetTemp, heatingActive);
    _updateHumidSection(currentHumid, targetHumid, humidActive);
    _updateMotorSection(motorMinutesLeft, motorSecondsLeft, motorActive);
    _updateIncubationSection(currentDay, totalDays, incubationType);
    
    // Watchdog besleme - ekran güncellemesi sonunda
    esp_task_wdt_reset();
}

void Display::showMenu(String menuItems[], int itemCount, int selectedItem) {
    clear();
    
    // Watchdog besleme - menü gösterimi başlangıcında
    esp_task_wdt_reset();
    
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, 5);
    _tft.print("MENU");
    
    _tft.drawFastHLine(0, 15, SCREEN_WIDTH, COLOR_DIVISION);
    
    for (int i = 0; i < itemCount; i++) {
        if (i == selectedItem) {
            // Seçili menü öğesi için çerçeve çiz
            _tft.drawRect(0, 20 + i * 12, SCREEN_WIDTH, 12, COLOR_HIGHLIGHT);
            _tft.setTextColor(COLOR_HIGHLIGHT);
        } else {
            _tft.setTextColor(COLOR_TEXT);
        }
        
        _tft.setCursor(5, 22 + i * 12);
        _tft.print(menuItems[i]);
    }
    
    // Watchdog besleme - menü gösterimi sonunda
    esp_task_wdt_reset();
}

void Display::showSubmenu(String submenuItems[], int itemCount, int selectedItem) {
    clear();
    
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, 5);
    _tft.print("ALT MENU");
    
    _tft.drawFastHLine(0, 15, SCREEN_WIDTH, COLOR_DIVISION);
    
    for (int i = 0; i < itemCount; i++) {
        if (i == selectedItem) {
            // Seçili menü öğesi için çerçeve çiz
            _tft.drawRect(0, 20 + i * 12, SCREEN_WIDTH, 12, COLOR_HIGHLIGHT);
            _tft.setTextColor(COLOR_HIGHLIGHT);
        } else {
            _tft.setTextColor(COLOR_TEXT);
        }
        
        _tft.setCursor(5, 22 + i * 12);
        _tft.print(submenuItems[i]);
    }
}

void Display::showValueAdjustScreen(String title, String value, String unit) {
    clear();
    
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, 5);
    _tft.print(title);
    
    _tft.drawFastHLine(0, 15, SCREEN_WIDTH, COLOR_DIVISION);
    
    // Değer gösterimi
    _tft.setTextSize(2);
    _tft.setTextColor(COLOR_HIGHLIGHT);
    
    // Değerin boyutlarını ölç
    int16_t x1, y1;
    uint16_t w, h;
    String displayText = value + unit;
    _tft.getTextBounds(displayText, 0, 0, &x1, &y1, &w, &h);
    _tft.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT / 2 - h / 2);
    _tft.print(displayText);
    
    // Yönergeler
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, SCREEN_HEIGHT - 30);
    _tft.print("Joystick: Yukari/Asagi");
    _tft.setCursor(5, SCREEN_HEIGHT - 20);
    _tft.print("Buton: Onayla");
}

void Display::showValueAdjustScreen(String title, float value, String unit) {
    // Float değeri String'e dönüştür ve String versiyonunu çağır
    showValueAdjustScreen(title, String(value, 1), unit);
}

void Display::showConfirmationMessage(String message) {
    _tft.fillRect(20, SCREEN_HEIGHT / 2 - 20, SCREEN_WIDTH - 40, 40, COLOR_BACKGROUND);
    _tft.drawRect(20, SCREEN_HEIGHT / 2 - 20, SCREEN_WIDTH - 40, 40, COLOR_HIGHLIGHT);
    
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_HIGHLIGHT);
    
    // Mesajın boyutlarını ölç
    int16_t x1, y1;
    uint16_t w, h;
    _tft.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);
    _tft.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT / 2 - h / 2);
    _tft.print(message);
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    delay(2000); // 2 saniye göster
    
    // Uzun beklemeden sonra watchdog besleme
    esp_task_wdt_reset();
}

void Display::showAlarmMessage(String alarmType, String alarmValue) {
    _tft.fillRect(10, SCREEN_HEIGHT / 2 - 25, SCREEN_WIDTH - 20, 50, COLOR_BACKGROUND);
    _tft.drawRect(10, SCREEN_HEIGHT / 2 - 25, SCREEN_WIDTH - 20, 50, COLOR_ALARM);
    
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_ALARM);
    
    // Alarm başlığı
    _tft.setCursor(20, SCREEN_HEIGHT / 2 - 15);
    _tft.print("ALARM");
    
    // Alarm tipi
    _tft.setCursor(20, SCREEN_HEIGHT / 2);
    _tft.print(alarmType);
    
    // Alarm değeri
    _tft.setCursor(20, SCREEN_HEIGHT / 2 + 15);
    _tft.print(alarmValue);
}

void Display::showProgressBar(int x, int y, int width, int height, uint16_t color, int percentage) {
    // Yüzde değerini doğrula (0-100 arasında sınırla)
    percentage = constrain(percentage, 0, 100);
    
    // Çerçeve
    _tft.drawRect(x, y, width, height, COLOR_TEXT);
    
    // İçeriği temizle
    _tft.fillRect(x + 1, y + 1, width - 2, height - 2, COLOR_BACKGROUND);
    
    // İlerleme değerini göster
    if (percentage > 0) {
        int fillWidth = ((width - 2) * percentage) / 100;
        _tft.fillRect(x + 1, y + 1, fillWidth, height - 2, color);
        
        // Yüzde değerini metin olarak göster (ilerleme çubuğu yeterince genişse)
        if (width > 40) {
            char percentText[5];
            sprintf(percentText, "%d%%", percentage);
            
            // Metin konumu ayarları
            _tft.setTextSize(1);
            _tft.setTextColor(COLOR_TEXT);
            
            // Metin boyutunu ölç
            int16_t x1, y1;
            uint16_t w, h;
            _tft.getTextBounds(percentText, 0, 0, &x1, &y1, &w, &h);
            
            // Metni ilerleme çubuğunun ortasına yerleştir
            _tft.setCursor(x + (width - w) / 2, y + (height - h) / 2 + 1);
            _tft.print(percentText);
        }
    }
}