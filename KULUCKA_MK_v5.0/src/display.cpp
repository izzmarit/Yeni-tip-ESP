/**
 * @file display.cpp
 * @brief TFT LCD ekran yönetimi uygulaması
 * @version 1.3
 */

#include "display.h"
#include "menu.h"  // MenuManager için gerekli include

Display::Display() : _tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST) {
    // Yapılandırıcı
    _currentMode = DISPLAY_NONE;
    _menuChanged = false;
    _lastSelectedItem = -1;
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
    _currentMode = DISPLAY_SPLASH;
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
    _currentMode = DISPLAY_MAIN;
    _tft.fillScreen(COLOR_BACKGROUND);
    _drawDividers();
    
    // Bölüm başlıklarını göster
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    
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
    
    // Son değerleri sıfırla (zorunlu güncelleme)
    _lastTemp = -999;
    _lastTargetTemp = -999;
    _lastHumid = -999;
    _lastTargetHumid = -999;
    _lastMotorMin = -1;
    _lastMotorSec = -1;
    _lastDay = -1;
    _lastTotalDays = -1;
    _lastHeatingActive = false;
    _lastHumidActive = false;
    _lastMotorActive = false;
    _lastTimeStr = "";
    _lastDateStr = "";
    _lastType = "";
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
    // Sadece saat veya tarih değiştiyse güncelle
    if (timeStr != _lastTimeStr || dateStr != _lastDateStr) {
        // Saat, MK v5.0 metni, tarih bilgilerini güncelle
        _tft.fillRect(0, 0, SCREEN_WIDTH, 15, COLOR_BACKGROUND);
        
        _tft.setTextSize(1);
        _tft.setTextColor(COLOR_TEXT);
        
        // Saat
        _tft.setCursor(2, 5);
        _tft.print(timeStr);
        
        // Tarih - ekranın üst kısmında sağ tarafa hizalama
        int16_t x1, y1;
        uint16_t w, h;
        _tft.getTextBounds(dateStr, 0, 0, &x1, &y1, &w, &h);
        _tft.setCursor(SCREEN_WIDTH - w - 3, 5);
        _tft.print(dateStr);
        
        // MK v5.0 - saat ve tarih arasında ortala
        uint16_t timeWidth, dateWidth;
        _tft.getTextBounds(timeStr, 0, 0, &x1, &y1, &timeWidth, &h);
        _tft.getTextBounds(dateStr, 0, 0, &x1, &y1, &dateWidth, &h);
        
        _tft.getTextBounds("MK v5.0", 0, 0, &x1, &y1, &w, &h);
        int centerX = (SCREEN_WIDTH - (dateWidth + timeWidth + 6)) / 2 + timeWidth + 3;
        _tft.setCursor(centerX - w/2, 5);
        _tft.print("MK v5.0");
        
        // Son değerleri güncelle
        _lastTimeStr = timeStr;
        _lastDateStr = dateStr;
    }
}

void Display::_updateTempSection(float currentTemp, float targetTemp, bool heatingActive) {
    // Sıcaklık değeri değiştiyse güncelle
    bool shouldUpdate = abs(currentTemp - _lastTemp) >= 0.1 || 
                         targetTemp != _lastTargetTemp || 
                         heatingActive != _lastHeatingActive;
    
    if (shouldUpdate) {
        // Sıcaklık bölmesini temizle
        _tft.fillRect(1, 16, SCREEN_WIDTH / 2 - 1, (SCREEN_HEIGHT - 15) / 2 - 1, COLOR_BACKGROUND);
        
        _tft.setTextSize(1);
        
        // SICAKLIK başlığı - DÜZELTME: Yanıp sönme yerine sabit renk
        if (heatingActive) {
            _tft.setTextColor(COLOR_TEMP); // Kırmızı
        } else {
            _tft.setTextColor(COLOR_TEXT); // Beyaz
        }
        
        _tft.setCursor(16, 20);
        _tft.print("SICAKLIK");
        
        // Hedef sıcaklık - aşağı kaydırıldı, alt çizgiye yakın ama değmiyor
        _tft.setTextColor(COLOR_TARGET);
        _tft.setCursor(5, 60);  // Y pozisyonu
        _tft.print("Hedef:");
        _tft.print(targetTemp, 1);
        _tft.print("C");
        _tft.write(247); // Derece sembolü
        
        // Mevcut sıcaklık
        _tft.setTextSize(2);
        _tft.setTextColor(COLOR_TEMP);
        
        char tempStr[7];
        sprintf(tempStr, "%4.1f", currentTemp);
        
        // Mevcut sıcaklığın boyutlarını ölç ve tamamen ortala
        int16_t x1, y1;
        uint16_t w, h;
        String fullStr = String(tempStr) + "C" + char(247);
        _tft.getTextBounds(fullStr, 0, 0, &x1, &y1, &w, &h);
        
        // Yatay ve dikey olarak ortala, orta çizgiden biraz uzakta
        _tft.setCursor((SCREEN_WIDTH / 2 - w) / 2 - 2, 37);
        _tft.print(tempStr);
        
        // Derece sembolü
        _tft.setCursor(_tft.getCursorX(), 37);
        _tft.print("C");
        _tft.write(247); // Derece sembolü
        
        // Son değerleri güncelle
        _lastTemp = currentTemp;
        _lastTargetTemp = targetTemp;
        _lastHeatingActive = heatingActive;
    }
}

void Display::_updateHumidSection(float currentHumid, float targetHumid, bool humidActive) {
    // Nem değeri değiştiyse güncelle
    bool shouldUpdate = abs(currentHumid - _lastHumid) >= 0.5 || 
                         targetHumid != _lastTargetHumid || 
                         humidActive != _lastHumidActive;
    
    if (shouldUpdate) {
        // Nem bölmesini temizle
        _tft.fillRect(SCREEN_WIDTH / 2 + 1, 16, SCREEN_WIDTH / 2 - 1, (SCREEN_HEIGHT - 15) / 2 - 1, COLOR_BACKGROUND);
        
        _tft.setTextSize(1);
        
        // NEM başlığı - DÜZELTME: Yanıp sönme yerine sabit renk
        if (humidActive) {
            _tft.setTextColor(COLOR_HUMID); // Mavi
        } else {
            _tft.setTextColor(COLOR_TEXT); // Beyaz
        }
        
        _tft.setCursor(103, 20);
        _tft.print("NEM");
        
        // Hedef nem - aşağı kaydırıldı, alt çizgiye yakın ama değmiyor
        _tft.setTextColor(COLOR_TARGET);
        _tft.setCursor(85, 60);  // Y pozisyonu
        _tft.print("Hedef:%");
        _tft.print(targetHumid, 0);
        
        // Mevcut nem
        _tft.setTextSize(2);
        _tft.setTextColor(COLOR_HUMID);
        
        char humidStr[6];
        sprintf(humidStr, "%3.0f", currentHumid);
        
        // Mevcut nemin boyutlarını ölç ve sıcaklık göstergesiyle aynı hizaya getir
        int16_t x1, y1;
        uint16_t w, h;
        String fullStr = String(humidStr) + "%";
        _tft.getTextBounds(fullStr, 0, 0, &x1, &y1, &w, &h);
        
        // Sıcaklıkla aynı yükseklikte ve kendi bölgesinde ortala
        _tft.setCursor(SCREEN_WIDTH / 2 + (SCREEN_WIDTH / 2 - w) / 2, 37);
        _tft.print(humidStr);
        
        // Yüzde sembolü
        _tft.setCursor(_tft.getCursorX(), 37);
        _tft.print("%");
        
        // Son değerleri güncelle
        _lastHumid = currentHumid;
        _lastTargetHumid = targetHumid;
        _lastHumidActive = humidActive;
    }
}

// Özel saat ayarlama ekranı gösterimi için showValueAdjustScreen'i genişlet
void Display::showTimeAdjustScreen(String title, String timeString, int selectedField) {
    // Ekran modu değişti
    _currentMode = DISPLAY_VALUE_ADJUST;
    
    clear();
    
    // Başlık
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, 5);
    _tft.print(title);
    _tft.drawFastHLine(0, 15, SCREEN_WIDTH, COLOR_DIVISION);
    
    // Saat gösterimi - büyük ve merkeze
    _tft.setTextSize(3);
    
    // Saat ve dakikayı ayrı ayrı göster, seçili alanı vurgula
    String hourStr = timeString.substring(0, 2);
    String minuteStr = timeString.substring(3, 5);
    
    int16_t x1, y1;
    uint16_t w, h;
    
    // Saat kısmı
    if (selectedField == 0) {
        _tft.setTextColor(COLOR_HIGHLIGHT); // Seçili alan vurgulanır
    } else {
        _tft.setTextColor(COLOR_TEXT);
    }
    _tft.setCursor(30, SCREEN_HEIGHT / 2 - 10);
    _tft.print(hourStr);
    
    // İki nokta
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(66, SCREEN_HEIGHT / 2 - 10);
    _tft.print(":");
    
    // Dakika kısmı
    if (selectedField == 1) {
        _tft.setTextColor(COLOR_HIGHLIGHT); // Seçili alan vurgulanır
    } else {
        _tft.setTextColor(COLOR_TEXT);
    }
    _tft.setCursor(84, SCREEN_HEIGHT / 2 - 10);
    _tft.print(minuteStr);
    
    // Yönergeler
    _tft.fillRect(0, SCREEN_HEIGHT - 15, SCREEN_WIDTH, 15, COLOR_DIVISION);
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, SCREEN_HEIGHT - 13);
    _tft.print("^v:Deger <>:Alan *:Kaydet");
    
    _needsFullRedraw = false;
}

void Display::showDateAdjustScreen(String title, String dateString, int selectedField) {
    // Ekran modu değişti
    _currentMode = DISPLAY_VALUE_ADJUST;
    
    clear();
    
    // Başlık
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, 5);
    _tft.print(title);
    _tft.drawFastHLine(0, 15, SCREEN_WIDTH, COLOR_DIVISION);
    
    // Tarih gösterimi - büyük ve merkeze
    _tft.setTextSize(2);
    
    // Gün, ay, yılı ayrı ayrı göster, seçili alanı vurgula
    String dayStr = dateString.substring(0, 2);
    String monthStr = dateString.substring(3, 5);
    String yearStr = dateString.substring(6, 10);
    
    // Gün kısmı
    if (selectedField == 0) {
        _tft.setTextColor(COLOR_HIGHLIGHT);
    } else {
        _tft.setTextColor(COLOR_TEXT);
    }
    _tft.setCursor(10, SCREEN_HEIGHT / 2 - 10);
    _tft.print(dayStr);
    
    // Nokta
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(34, SCREEN_HEIGHT / 2 - 10);
    _tft.print(".");
    
    // Ay kısmı
    if (selectedField == 1) {
        _tft.setTextColor(COLOR_HIGHLIGHT);
    } else {
        _tft.setTextColor(COLOR_TEXT);
    }
    _tft.setCursor(42, SCREEN_HEIGHT / 2 - 10);
    _tft.print(monthStr);
    
    // Nokta
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(66, SCREEN_HEIGHT / 2 - 10);
    _tft.print(".");
    
    // Yıl kısmı
    if (selectedField == 2) {
        _tft.setTextColor(COLOR_HIGHLIGHT);
    } else {
        _tft.setTextColor(COLOR_TEXT);
    }
    _tft.setCursor(74, SCREEN_HEIGHT / 2 - 10);
    _tft.print(yearStr);
    
    // Yönergeler
    _tft.fillRect(0, SCREEN_HEIGHT - 15, SCREEN_WIDTH, 15, COLOR_DIVISION);
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, SCREEN_HEIGHT - 13);
    _tft.print("^v:Deger <>:Alan *:Kaydet");
    
    _needsFullRedraw = false;
}

void Display::_updateMotorSection(int minutesLeft, int secondsLeft, bool motorActive) {
    // Motor durumu veya zamanlaması değiştiyse güncelle
    static unsigned long lastBlinkMillis = 0;
    bool shouldUpdate = (minutesLeft != _lastMotorMin) || 
                        (secondsLeft != _lastMotorSec) || 
                        (motorActive != _lastMotorActive);
    
    // Yanıp sönme için düzenli güncelleme
    if (motorActive) {
        unsigned long currentMillis = millis();
        if (currentMillis - lastBlinkMillis >= 500) {
            shouldUpdate = true;
            lastBlinkMillis = currentMillis;
        }
    }
    
    if (shouldUpdate) {
        // Motor bölmesini temizle
        _tft.fillRect(1, (SCREEN_HEIGHT - 15) / 2 + 16, SCREEN_WIDTH / 2 - 1, (SCREEN_HEIGHT - 15) / 2 - 1, COLOR_BACKGROUND);
        
        _tft.setTextSize(1);
        
        // MOTOR başlığı (yanıp sönme efekti için)
        if (motorActive) {
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
        
        // Dakika ve saniye gösterimi
        _tft.setTextSize(1);
        
        // Dakika gösterimi - büyük rakamlar
        _tft.setCursor(5, 90);
        _tft.print("Dk:");
        
        _tft.setTextSize(2);
        _tft.setCursor(25, 88);
        _tft.print(minutesLeft);
        
        // Saniye gösterimi
        _tft.setTextSize(1);
        _tft.setCursor(5, 110);
        _tft.print("Sn:");
        
        _tft.setTextSize(2);
        _tft.setCursor(25, 108);
        
        // Saniye değerini göster - motor aktif veya pasif olsun her durumda
        _tft.print(secondsLeft);
        
        // Son değerleri güncelle
        _lastMotorMin = minutesLeft;
        _lastMotorSec = secondsLeft;
        _lastMotorActive = motorActive;
    }
}

void Display::_updateIncubationSection(int currentDay, int totalDays, String incubationType) {
    // Kuluçka bilgisi değiştiyse güncelle
    if (currentDay != _lastDay || totalDays != _lastTotalDays || incubationType != _lastType) {
        // Kuluçka bölmesini temizle
        _tft.fillRect(SCREEN_WIDTH / 2 + 1, (SCREEN_HEIGHT - 15) / 2 + 16, SCREEN_WIDTH / 2 - 1, (SCREEN_HEIGHT - 15) / 2 - 1, COLOR_BACKGROUND);
        
        _tft.setTextSize(1);
        _tft.setTextColor(COLOR_TEXT);
        
        // KULUCKA başlığı
        _tft.setCursor(95, 74);
        _tft.print("KULUCKA");
        
        // Kuluçka tipi - konumu aşağı kaydırıldı ve rengi sarı yapıldı
        _tft.setTextColor(COLOR_TARGET);  // Sarı renk
        _tft.setCursor(85, 115);  // Y pozisyonu 105'ten 115'e kaydırıldı
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
        
        // Son değerleri güncelle
        _lastDay = currentDay;
        _lastTotalDays = totalDays;
        _lastType = incubationType;
    }
}

// Ana ekranı güncelle
void Display::updateMainScreen(float currentTemp, float targetTemp, float currentHumid, 
                               float targetHumid, int motorMinutesLeft, int motorSecondsLeft, 
                               int currentDay, int totalDays, String incubationType,
                               bool heatingActive, bool humidActive, bool motorActive,
                               String timeStr, String dateStr) {
    
    // Ana ekranda değilsek hiçbir şey yapma
    if (_currentMode != DISPLAY_MAIN) {
        return;
    }
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    // Menüden dönüşte ilk ekran çizimini zorla
    static unsigned long lastDrawTime = 0;
    static bool forceDraw = false;
    
    if (millis() - lastDrawTime > 2000) { // 2 saniye güncellemeden sonra
        forceDraw = true;
    }
    lastDrawTime = millis();
    
    // İlk seferde veya ekranın yeniden oluşturulması gerektiğinde tam çizim yap
    if (_needsFullRedraw || forceDraw) {
        _tft.fillScreen(COLOR_BACKGROUND);
        _drawDividers();
        _needsFullRedraw = false;
        forceDraw = false;
        
        // Son değerleri sıfırla (zorunlu güncelleme)
        _lastTemp = -999;
        _lastTargetTemp = -999;
        _lastHumid = -999;
        _lastTargetHumid = -999;
        _lastMotorMin = -1;
        _lastMotorSec = -1;
        _lastDay = -1;
        _lastTotalDays = -1;
        _lastHeatingActive = !heatingActive; // Zorunlu güncelleme
        _lastHumidActive = !humidActive;    // Zorunlu güncelleme
        _lastMotorActive = !motorActive;    // Zorunlu güncelleme
        _lastTimeStr = "";                 // Zorunlu güncelleme
        _lastDateStr = "";                 // Zorunlu güncelleme
        _lastType = "";                    // Zorunlu güncelleme
    }
    
    // Sadece bilgi çubuğunu güncelle (saat her zaman değişir)
    _updateInfoBar(timeStr, dateStr);
    
    // Her bölümü ayrı ayrı güncelle - sadece değişen veya yanıp sönen bölümleri güncelle
    _updateTempSection(currentTemp, targetTemp, heatingActive);
    _updateHumidSection(currentHumid, targetHumid, humidActive);
    _updateMotorSection(motorMinutesLeft, motorSecondsLeft, motorActive);
    _updateIncubationSection(currentDay, totalDays, incubationType);
    
    // Watchdog besleme
    esp_task_wdt_reset();
}

void Display::showMenu(String menuItems[], int itemCount, int selectedItem) {
    // Ekran modu değişti
    _currentMode = DISPLAY_MENU;
    
    // İTEM SAYISI SIFIR ISE HİÇBİR ŞEY YAPMA - KRİTİK DÜZELTME
    if (itemCount <= 0) {
        Serial.println("Boş menü tespit edildi, menü gösterilmiyor");
        return;
    }
    
    // MenuManager referansını extern olarak al (main.cpp'den)
    extern MenuManager menuManager;
    int menuOffset = menuManager.getMenuOffset();
    
    // Menüyü her zaman yenile - bu satır kritik düzeltme
    clear();
    
    // Watchdog besleme - menü gösterimi başlangıcında
    esp_task_wdt_reset();
    
    // Üst menü başlığı
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, 5);
    _tft.print("MENU");
    _tft.drawFastHLine(0, 15, SCREEN_WIDTH, COLOR_DIVISION);
    
    // Menü listesi için uygun alan hesaplama
    int menuStartY = 16;
    int menuEndY = SCREEN_HEIGHT - 16;
    int menuHeight = menuEndY - menuStartY;
    
    // Maksimum görünebilir öğe sayısı
    const int maxVisibleItems = 6;
    int visibleItemCount = min(itemCount, maxVisibleItems);
    
    // Menü öğesi başına düşen yükseklik
    int itemHeight = menuHeight / visibleItemCount;
    
    // Sadece görünür aralıktaki öğeleri göster
    for (int i = 0; i < visibleItemCount; i++) {
        int actualIndex = i + menuOffset;
        
        // Dizin sınırlarını kontrol et
        if (actualIndex >= itemCount) break;
        
        if (actualIndex == selectedItem) {
            // Seçili menü öğesi için çerçeve çiz
            _tft.drawRect(0, menuStartY + i * itemHeight, SCREEN_WIDTH, itemHeight, COLOR_HIGHLIGHT);
            _tft.setTextColor(COLOR_HIGHLIGHT);
        } else {
            _tft.setTextColor(COLOR_TEXT);
        }
        
        // Menü öğesini yazdır
        _tft.setCursor(5, menuStartY + i * itemHeight + 2);
        _tft.print(menuItems[actualIndex]);
    }
    
    // Kaydırma göstergesi
    if (itemCount > maxVisibleItems) {
        // Yukarı ok (yukarıda daha fazla öğe varsa)
        if (menuOffset > 0) {
            _tft.setTextColor(COLOR_HIGHLIGHT);
            _tft.setCursor(SCREEN_WIDTH - 10, menuStartY - 5);
            _tft.print("^");
        }
        
        // Aşağı ok (aşağıda daha fazla öğe varsa)
        if (menuOffset + maxVisibleItems < itemCount) {
            _tft.setTextColor(COLOR_HIGHLIGHT);
            _tft.setCursor(SCREEN_WIDTH - 10, menuEndY + 2);
            _tft.print("v");
        }
    }
    
    // Kontrol ipuçları - alt kısımda
    _tft.fillRect(0, SCREEN_HEIGHT - 15, SCREEN_WIDTH, 15, COLOR_DIVISION);
    _tft.setCursor(5, SCREEN_HEIGHT - 13);
    _tft.setTextColor(COLOR_TEXT);
    _tft.print("^v:Sec <:Geri >:Giris");
    
    // Son seçili öğeyi güncelle
    _lastSelectedItem = selectedItem;
    _menuChanged = false;
    
    // Watchdog besleme - menü gösterimi sonunda
    esp_task_wdt_reset();
}

void Display::showSubmenu(String submenuItems[], int itemCount, int selectedItem) {
    // Ekran modu değişti
    _currentMode = DISPLAY_SUBMENU;
    
    // Seçili öğe değişmediyse ve menü içeriği değişmediyse tekrar çizme
    if (selectedItem == _lastSelectedItem && !_menuChanged) {
        return;
    }
    
    clear();
    
    // Üst alt menü başlığı
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, 5);
    _tft.print("ALT MENU");
    _tft.drawFastHLine(0, 15, SCREEN_WIDTH, COLOR_DIVISION);
    
    // Menü listesi için kullanılabilir alan
    // Üst başlık (15px) ve alt navigasyon (15px) alanı dışında kalan alan
    int menuStartY = 16;
    int menuEndY = SCREEN_HEIGHT - 16;
    int menuHeight = menuEndY - menuStartY;
    
    // Menü öğesi başına düşen yükseklik (en fazla 8 öğe sığabilir)
    int itemHeight = menuHeight / min(itemCount, 8);
    
    for (int i = 0; i < itemCount; i++) {
        if (i == selectedItem) {
            // Seçili menü öğesi için çerçeve çiz
            _tft.drawRect(0, menuStartY + i * itemHeight, SCREEN_WIDTH, itemHeight, COLOR_HIGHLIGHT);
            _tft.setTextColor(COLOR_HIGHLIGHT);
        } else {
            _tft.setTextColor(COLOR_TEXT);
        }
        
        // Menü öğesini yazdır
        _tft.setCursor(5, menuStartY + i * itemHeight + 2);
        _tft.print(submenuItems[i]);
    }
    
    // Kontrol ipuçları - alt kısımda
    _tft.fillRect(0, SCREEN_HEIGHT - 15, SCREEN_WIDTH, 15, COLOR_DIVISION);
    _tft.setCursor(5, SCREEN_HEIGHT - 13);
    _tft.setTextColor(COLOR_TEXT);
    _tft.print("^v:Sec <:Geri >:Sec");
    
    // Son seçili öğeyi güncelle
    _lastSelectedItem = selectedItem;
    _menuChanged = false;
}

void Display::showValueAdjustScreen(String title, String value, String unit) {
    // Ekran modu değişti
    _currentMode = DISPLAY_VALUE_ADJUST;
    
    // Son değeri kaydet - değişiklik kontrolü için
    static String lastValue = "";
    
    // Değer değişmediyse tekrar çizme
    if (value == lastValue && !_needsFullRedraw) {
        return;
    }
    
    clear();
    
    // Başlık
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, 5);
    _tft.print(title);
    _tft.drawFastHLine(0, 15, SCREEN_WIDTH, COLOR_DIVISION);
    
    // Değer gösterimi - daha büyük ve merkeze
    _tft.setTextSize(2);
    _tft.setTextColor(COLOR_HIGHLIGHT);
    
    // Değerin boyutlarını ölç
    int16_t x1, y1;
    uint16_t w, h;
    String displayText = value + unit;
    _tft.getTextBounds(displayText, 0, 0, &x1, &y1, &w, &h);
    _tft.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT / 2 - h);
    _tft.print(displayText);
    
    // Artı/eksi göstergeleri
    _tft.setTextSize(2);
    _tft.setCursor(30, SCREEN_HEIGHT / 2 + 10);
    _tft.print("-");
    
    _tft.setCursor(SCREEN_WIDTH - 40, SCREEN_HEIGHT / 2 + 10);
    _tft.print("+");
    
    // Yönergeler
    _tft.fillRect(0, SCREEN_HEIGHT - 15, SCREEN_WIDTH, 15, COLOR_DIVISION);
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, SCREEN_HEIGHT - 13);
    _tft.print("^v:Deger <:Geri >:Kaydet");
    
    lastValue = value;
    _needsFullRedraw = false;
}

void Display::showValueAdjustScreen(String title, float value, String unit) {
    // Float değeri String'e dönüştür ve String versiyonunu çağır
    showValueAdjustScreen(title, String(value, 1), unit);
}

void Display::showSensorValuesScreen(float temp1, float humid1, float temp2, float humid2, bool sensor1Working, bool sensor2Working) {
    // Ekran modu değişti
    _currentMode = DISPLAY_VALUE_ADJUST;
    
    clear();
    
    // Başlık
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, 5);
    _tft.print("SENSOR DEGERLERI");
    _tft.drawFastHLine(0, 15, SCREEN_WIDTH, COLOR_DIVISION);
    
    // Sensör 1 bilgileri
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, 25);
    _tft.print("SENSOR 1:");
    
    if (sensor1Working) {
        _tft.setTextColor(COLOR_TEMP);
        _tft.setCursor(5, 40);
        _tft.print("Sicaklik: ");
        _tft.print(temp1, 1);
        _tft.print("C");
        _tft.write(247);
        
        _tft.setTextColor(COLOR_HUMID);
        _tft.setCursor(5, 55);
        _tft.print("Nem: ");
        _tft.print(humid1, 1);
        _tft.print("%");
    } else {
        _tft.setTextColor(COLOR_ALARM);
        _tft.setCursor(5, 40);
        _tft.print("CALISMIOR!");
    }
    
    // Sensör 2 bilgileri
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, 75);
    _tft.print("SENSOR 2:");
    
    if (sensor2Working) {
        _tft.setTextColor(COLOR_TEMP);
        _tft.setCursor(5, 90);
        _tft.print("Sicaklik: ");
        _tft.print(temp2, 1);
        _tft.print("C");
        _tft.write(247);
        
        _tft.setTextColor(COLOR_HUMID);
        _tft.setCursor(5, 105);
        _tft.print("Nem: ");
        _tft.print(humid2, 1);
        _tft.print("%");
    } else {
        _tft.setTextColor(COLOR_ALARM);
        _tft.setCursor(5, 90);
        _tft.print("CALISMIOR!");
    }
    
    // Yönergeler
    _tft.fillRect(0, SCREEN_HEIGHT - 15, SCREEN_WIDTH, 15, COLOR_DIVISION);
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, SCREEN_HEIGHT - 13);
    _tft.print("<:Geri");
    
    _needsFullRedraw = false;
}

void Display::showConfirmationMessage(String message) {
    // Ekran modu değişti
    _currentMode = DISPLAY_CONFIRMATION;
    
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
    
    // Ana ekrana dönüşte tam çizimi zorla
    _needsFullRedraw = true;
    
    // Uzun beklemeden sonra watchdog besleme
    esp_task_wdt_reset();
}

void Display::showAlarmMessage(String alarmType, String alarmValue) {
    // Ekran modu değişti
    _currentMode = DISPLAY_ALARM;
    
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
    
    // Ana ekrana dönüşte tam çizimi zorla
    _needsFullRedraw = true;
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

// Menü öğelerinin değiştiğini belirt
void Display::setMenuChanged() {
    _menuChanged = true;
}

// Mevcut ekran modunu al
DisplayMode Display::getCurrentMode() {
    return _currentMode;
}

// PID durum ekranı göster
void Display::showPIDStatusScreen(String pidMode, String pidValues) {
    _currentMode = DISPLAY_VALUE_ADJUST;
    
    clear();
    
    // Başlık
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, 5);
    _tft.print("PID DURUMU");
    _tft.drawFastHLine(0, 15, SCREEN_WIDTH, COLOR_DIVISION);
    
    // PID modu
    _tft.setTextSize(2);
    _tft.setTextColor(COLOR_HIGHLIGHT);
    _tft.setCursor(10, 30);
    _tft.print("Mod:");
    
    _tft.setTextColor(COLOR_TARGET);
    _tft.setCursor(10, 50);
    _tft.print(pidMode);
    
    // PID değerleri
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(10, 75);
    _tft.print(pidValues);
    
    // Yönergeler
    _tft.fillRect(0, SCREEN_HEIGHT - 15, SCREEN_WIDTH, 15, COLOR_DIVISION);
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setCursor(5, SCREEN_HEIGHT - 13);
    _tft.print("<:Geri");
    
    _needsFullRedraw = false;
}