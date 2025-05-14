/**
 * @file main.cpp
 * @brief KULUÇKA MK v5.0 ana uygulama dosyası
 * @version 1.1
 */

#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "sensors.h"
#include "rtc.h"
#include "joystick.h"
#include "relays.h"
#include "incubation.h"
#include "pid.h"
#include "hysteresis.h"
#include "menu.h"
#include "storage.h"
#include "wifi_manager.h"
#include "alarm.h"

// Modül nesneleri
Display display;
Sensors sensors;
RTCModule rtc;
Joystick joystick;
Relays relays;
Incubation incubation;
PIDController pidController;
Hysteresis hysteresisController;
MenuManager menuManager;
Storage storage;
WiFiManager wifiManager;
AlarmManager alarmManager;

// Zaman kontrolü değişkenleri
unsigned long lastSensorReadTime = 0;
unsigned long lastDisplayUpdateTime = 0;
unsigned long lastJoystickReadTime = 0;
unsigned long lastMenuTimeout = 0;
unsigned long lastWdtFeedTime = 0;

// Menü zaman aşımı (ms) - 30 saniye
const unsigned long MENU_TIMEOUT = 30000;

// Watchdog besleme aralığı (ms) - 3 saniye
const unsigned long WDT_FEED_INTERVAL = 3000;

// Fonksiyon prototipleri
void initializeModules();
void handleJoystick();
void updateSensors();
void updateDisplay();
void updateRelays();
void updateAlarm();
void handleMenuActions(JoystickDirection direction);
void loadSettingsFromStorage();
void saveSettingsToStorage();
void handleValueAdjustment(JoystickDirection direction);
void setupWatchdog();
void feedWatchdog();

void setup() {
    // Seri port başlatma
    Serial.begin(115200);
    Serial.println("KULUCKA MK v5.0 Baslatiliyor...");
    
    // Watchdog timer'ı başlat
    setupWatchdog();
    
    // Tüm modülleri başlat
    initializeModules();
    
    // Stored ayarları yükle
    loadSettingsFromStorage();
    
    // Açılış ekranını göster
    display.showSplashScreen();
    feedWatchdog(); // Açılış ekranı gösterilirken watchdog besleme
    
    // Ana ekranı ayarla
    display.setupMainScreen();
    
    Serial.println("KULUCKA MK v5.0 Hazir!");
    feedWatchdog();
}

void loop() {
    // Mevcut zaman
    unsigned long currentMillis = millis();
    
    // Düzenli watchdog beslemesi
    if (currentMillis - lastWdtFeedTime >= WDT_FEED_INTERVAL) {
        lastWdtFeedTime = currentMillis;
        feedWatchdog();
    }
    
    // Sensörleri güncelle
    if (currentMillis - lastSensorReadTime >= SENSOR_READ_DELAY) {
        lastSensorReadTime = currentMillis;
        updateSensors();
    }
    
    // Joystick kontrolü
    if (currentMillis - lastJoystickReadTime >= JOYSTICK_READ_DELAY) {
        lastJoystickReadTime = currentMillis;
        handleJoystick();
    }
    
    // Ekranı güncelle
    if (currentMillis - lastDisplayUpdateTime >= DISPLAY_REFRESH_DELAY) {
        lastDisplayUpdateTime = currentMillis;
        updateDisplay();
    }
    
    // Röleleri güncelle
    updateRelays();
    
    // Alarm durumunu güncelle
    updateAlarm();
    
    // Menü zaman aşımını kontrol et
    if (!menuManager.isInHomeScreen() && 
        (currentMillis - menuManager.getLastInteractionTime() >= MENU_TIMEOUT)) {
        menuManager.returnToHome();
    }
    
    // WiFi isteklerini işle
    wifiManager.handleRequests();
}

void setupWatchdog() {
    // Watchdog timer'ı başlat
    Serial.println("Watchdog Timer baslatiliyor...");
    esp_task_wdt_init(WDT_TIMEOUT, WDT_PANIC_MODE);
    esp_task_wdt_add(NULL);  // Mevcut görevi (NULL = ana görev) watchdog'a ekle
    Serial.println("Watchdog Timer baslatildi.");
}

void feedWatchdog() {
    // Watchdog timer'ı besle
    esp_task_wdt_reset();
}

void initializeModules() {
    // Her modülü başlat ve hata kontrolünü yap
    feedWatchdog();
    
    // Ekran modülü
    if (!display.begin()) {
        Serial.println("Ekran başlatma hatası!");
    }
    feedWatchdog();
    
    // Sensör modülü
    if (!sensors.begin()) {
        Serial.println("Sensör başlatma hatası! En az bir sensör çalışmalı.");
    }
    feedWatchdog();
    
    // RTC modülü
    if (!rtc.begin()) {
        Serial.println("RTC başlatma hatası!");
    }
    feedWatchdog();
    
    // Joystick modülü
    if (!joystick.begin()) {
        Serial.println("Joystick başlatma hatası!");
    }
    feedWatchdog();
    
    // Röle modülü
    if (!relays.begin()) {
        Serial.println("Röle başlatma hatası!");
    }
    feedWatchdog();
    
    // Kuluçka modülü
    if (!incubation.begin()) {
        Serial.println("Kuluçka kontrolü başlatma hatası!");
    }
    feedWatchdog();
    
    // PID kontrolü
    if (!pidController.begin()) {
        Serial.println("PID kontrolü başlatma hatası!");
    }
    feedWatchdog();
    
    // Histerezis kontrolü
    if (!hysteresisController.begin()) {
        Serial.println("Histerezis kontrolü başlatma hatası!");
    }
    feedWatchdog();
    
    // Menü yönetimi
    if (!menuManager.begin()) {
        Serial.println("Menü yönetimi başlatma hatası!");
    }
    feedWatchdog();
    
    // Saklama yönetimi
    if (!storage.begin()) {
        Serial.println("Saklama yönetimi başlatma hatası!");
    }
    feedWatchdog();
    
    // WiFi yönetimi
    if (!wifiManager.beginAP()) {
        Serial.println("WiFi başlatma hatası!");
    } else {
        wifiManager.startServer();
    }
    feedWatchdog();
    
    // Alarm yönetimi
    if (!alarmManager.begin()) {
        Serial.println("Alarm yönetimi başlatma hatası!");
    }
    feedWatchdog();
}

void handleJoystick() {
    // Joystick durumunu güncelle
    joystick.update();
    
    // Joystick yönünü oku
    JoystickDirection direction = joystick.readDirection();
    
    // Eğer joystick hareket ettiyse
    if (direction != JOYSTICK_NONE) {
        // Menü durumuna göre işlem yap
        if (menuManager.isInValueAdjustScreen()) {
            // Değer ayarlama ekranında
            handleValueAdjustment(direction);
        } else {
            // Menüde veya ana ekranda
            handleMenuActions(direction);
        }
        feedWatchdog(); // Kullanıcı etkileşiminden sonra watchdog besleme
    }
}

void updateSensors() {
    // Sıcaklık ve nem değerlerini oku
    float temp = sensors.readTemperature();
    float humid = sensors.readHumidity();
    
    // PID kontrolü için sıcaklık değerini kullan
    pidController.compute(temp);
    
    // Histerezis kontrolü için nem değerini kullan
    hysteresisController.compute(humid);
    
    // Kuluçka durumunu güncelle
    incubation.update(rtc.getCurrentDateTime());
    
    // WiFi durum verilerini güncelle
    wifiManager.updateStatusData(
        temp, 
        humid, 
        relays.getHeaterState(), 
        relays.getHumidifierState(), 
        relays.getMotorState(), 
        incubation.getCurrentDay(rtc.getCurrentDateTime()), 
        incubation.getTotalDays(), 
        incubation.getIncubationTypeName(), 
        incubation.getTargetTemperature(), 
        incubation.getTargetHumidity()
    );
    
    // Alarmlari kontrol et
    alarmManager.checkAlarms(
        temp, 
        incubation.getTargetTemperature(), 
        humid, 
        incubation.getTargetHumidity(), 
        relays.getMotorState(), 
        true,  // Motor zamanı doğru mu kontrolü yapılmalı
        sensors.areSensorsWorking()
    );
    
    feedWatchdog(); // Sensör güncellemesinden sonra watchdog besleme
}

void updateDisplay() {
    // Mevcut duruma göre ekranı güncelle
    if (menuManager.isInHomeScreen()) {
        // Ana ekran güncellemesi
        display.updateMainScreen(
            sensors.readTemperature(),
            incubation.getTargetTemperature(),
            sensors.readHumidity(),
            incubation.getTargetHumidity(),
            relays.getMotorWaitTimeLeft(),
            relays.getMotorRunTimeLeft(),
            incubation.getCurrentDay(rtc.getCurrentDateTime()),
            incubation.getTotalDays(),
            incubation.getIncubationTypeName(),
            relays.getHeaterState(),
            relays.getHumidifierState(),
            relays.getMotorState(),
            rtc.getTimeString(),
            rtc.getDateString()
        );
    } else if (menuManager.isInMenu()) {
        // Menü ekranı güncellemesi
        display.showMenu(
            menuManager.getMenuItems().data(),
            menuManager.getMenuItems().size(),
            menuManager.getSelectedIndex()
        );
    }
    
    // Alarm varsa alarm mesajını göster
    if (alarmManager.getCurrentAlarm() != ALARM_NONE) {
        display.showAlarmMessage(
            alarmManager.getAlarmMessage(),
            "Kontrol Et!"
        );
    }
    
    feedWatchdog(); // Ekran güncellemesinden sonra watchdog besleme
}

void updateRelays() {
    // PID çıkışına göre ısıtıcı rölesini kontrol et
    relays.setHeater(pidController.isOutputActive());
    
    // Histerezis çıkışına göre nem rölesini kontrol et
    relays.setHumidifier(hysteresisController.getOutput());
    
    // Motor rölesini güncelle
    relays.update();
}

void updateAlarm() {
    // Alarm yönetimini güncelle
    alarmManager.update();
}

void handleMenuActions(JoystickDirection direction) {
    // Menü durumunu güncelle
    menuManager.update(direction);
    
    // Mevcut menü durumuna göre işlem yap
    MenuState currentState = menuManager.getCurrentState();
    
    // Buton basıldığında seçili menü öğesine göre işlem yap
    if (joystick.wasButtonPressed()) {
        switch (currentState) {
            case MENU_INCUBATION_TYPE:
                if (menuManager.getSelectedIndex() < 3) {
                    // Kuluçka tipi seçimi (0: Tavuk, 1: Bıldırcın, 2: Kaz)
                    incubation.setIncubationType(menuManager.getSelectedIndex());
                    display.showConfirmationMessage("Tip kaydedildi");
                    saveSettingsToStorage();
                }
                break;
                
            case MENU_TEMPERATURE:
                // Sıcaklık ayarı
                menuManager.showValueAdjustScreen(
                    "Sicaklik Ayari",
                    incubation.getTargetTemperature(),
                    "C",
                    20.0,
                    40.0,
                    0.1
                );
                break;
                
            case MENU_HUMIDITY:
                // Nem ayarı
                menuManager.showValueAdjustScreen(
                    "Nem Ayari",
                    incubation.getTargetHumidity(),
                    "%",
                    30,
                    90,
                    1
                );
                break;
                
            case MENU_PID:
                // PID ayarı (seçili öğeye göre)
                if (menuManager.getSelectedIndex() == 0) {
                    menuManager.showValueAdjustScreen(
                        "PID Kp",
                        pidController.getKp(),
                        "",
                        0.0,
                        100.0,
                        0.1
                    );
                } else if (menuManager.getSelectedIndex() == 1) {
                    menuManager.showValueAdjustScreen(
                        "PID Ki",
                        pidController.getKi(),
                        "",
                        0.0,
                        100.0,
                        0.01
                    );
                } else if (menuManager.getSelectedIndex() == 2) {
                    menuManager.showValueAdjustScreen(
                        "PID Kd",
                        pidController.getKd(),
                        "",
                        0.0,
                        100.0,
                        0.1
                    );
                }
                break;
                
            case MENU_MOTOR_WAIT:
                // Motor bekleme süresi ayarı
                menuManager.showValueAdjustScreen(
                    "Bekleme Suresi",
                    relays.getMotorWaitTimeLeft(),
                    "dk",
                    1,
                    240,
                    1
                );
                break;
                
            case MENU_MOTOR_RUN:
                // Motor çalışma süresi ayarı
                menuManager.showValueAdjustScreen(
                    "Calisma Suresi",
                    relays.getMotorRunTimeLeft(),
                    "sn",
                    1,
                    60,
                    1
                );
                break;
                
            case MENU_SET_TIME:
                // Saat ayarı
                {
                    DateTime now = rtc.getCurrentDateTime();
                    menuManager.showValueAdjustScreen(
                        "Saat Ayari",
                        now.hour() * 100 + now.minute(),  // 1315 gibi bir değer (13:15)
                        "",
                        0,
                        2359,
                        1
                    );
                }
                break;
                
            case MENU_SET_DATE:
                // Tarih ayarı
                {
                    DateTime now = rtc.getCurrentDateTime();
                    menuManager.showValueAdjustScreen(
                        "Tarih Ayari",
                        now.day() * 1000000 + now.month() * 10000 + now.year(),  // 14052025 gibi bir değer (14.05.2025)
                        "",
                        1010000,
                        31122099,
                        1
                    );
                }
                break;
                
            case MENU_ADJUST_VALUE:
                // Değer ayarlama ekranında buton basılması - değeri kaydet
                if (menuManager.getCurrentState() == MENU_TEMPERATURE) {
                    // Değişikliği uygula
                    // TODO: Kuluçka tipine göre sıcaklık değişimi
                    
                    display.showConfirmationMessage("Deger kaydedildi");
                    saveSettingsToStorage();
                } else if (menuManager.getCurrentState() == MENU_HUMIDITY) {
                    // Değişikliği uygula
                    // TODO: Kuluçka tipine göre nem değişimi
                    
                    display.showConfirmationMessage("Deger kaydedildi");
                    saveSettingsToStorage();
                }
                // Diğer değer kayıtları için de benzer işlemler yapılmalı
                
                break;
                
            default:
                break;
        }
        
        feedWatchdog(); // Menü işlemlerinden sonra watchdog besleme
    }
}

void handleValueAdjustment(JoystickDirection direction) {
    // Ayarlanan değeri güncelle
    menuManager.update(direction);
    
    // Değeri göster
    display.showValueAdjustScreen(
        "Deger Ayarlama",
        String(menuManager.getAdjustedValue()), // Float'ı String'e dönüştür
        ""
    );
    
    // Buton basıldığında değer değişimini kaydet
    if (joystick.wasButtonPressed()) {
        // TODO: Değeri ilgili modüle kaydet (hangi değer ayarlandığına bağlı olarak)
        display.showConfirmationMessage("Deger kaydedildi");
        saveSettingsToStorage();
        feedWatchdog(); // Ayar kaydedildikten sonra watchdog besleme
    }
}

void loadSettingsFromStorage() {
    feedWatchdog(); // Ayarları yüklemeye başlamadan önce watchdog besleme
    
    // Ayarları saklama modülünden yükle
    incubation.setIncubationType(storage.getIncubationType());
    
    // Manüel kuluçka parametrelerini ayarla
    incubation.setManualParameters(
        storage.getManualDevTemp(),
        storage.getManualHatchTemp(),
        storage.getManualDevHumid(),
        storage.getManualHatchHumid(),
        storage.getManualDevDays(),
        storage.getManualHatchDays()
    );
    
    feedWatchdog(); // İşlem sırasında watchdog besleme
    
    // PID parametrelerini ayarla
    pidController.setTunings(
        storage.getPidKp(),
        storage.getPidKi(),
        storage.getPidKd()
    );
    
    // Kuluçka devam ediyor mu kontrolü
    if (storage.isIncubationRunning()) {
        incubation.startIncubation(storage.getStartTime());
    }
    
    // Hedef değerleri ayarla
    pidController.setSetpoint(incubation.getTargetTemperature());
    hysteresisController.setSetpoint(incubation.getTargetHumidity());
    
    feedWatchdog(); // İşlem sırasında watchdog besleme
    
    // Sensör kalibrasyonlarını ayarla
    sensors.setTemperatureCalibration(
        storage.getTempCalibration(0),
        storage.getTempCalibration(1)
    );
    
    sensors.setHumidityCalibration(
        storage.getHumidCalibration(0),
        storage.getHumidCalibration(1)
    );
    
    // Alarm eşiklerini ayarla
    alarmManager.setTempLowThreshold(storage.getTempLowAlarm());
    alarmManager.setTempHighThreshold(storage.getTempHighAlarm());
    alarmManager.setHumidLowThreshold(storage.getHumidLowAlarm());
    alarmManager.setHumidHighThreshold(storage.getHumidHighAlarm());
    
    feedWatchdog(); // Ayarları yükledikten sonra watchdog besleme
}

void saveSettingsToStorage() {
    feedWatchdog(); // Ayarları kaydetmeye başlamadan önce watchdog besleme
    
    // Ayarları saklama modülüne kaydet
    
    // Kuluçka türü
    storage.setIncubationType(incubation.getIncubationType());
    
    // Manuel kuluçka parametreleri
    IncubationParameters params = incubation.getParameters();
    if (incubation.getIncubationType() == INCUBATION_MANUAL) {
        storage.setManualDevTemp(params.developmentTemp);
        storage.setManualHatchTemp(params.hatchingTemp);
        storage.setManualDevHumid(params.developmentHumidity);
        storage.setManualHatchHumid(params.hatchingHumidity);
        storage.setManualDevDays(params.developmentDays);
        storage.setManualHatchDays(params.hatchingDays);
    }
    
    feedWatchdog(); // İşlem sırasında watchdog besleme
    
    // PID parametreleri
    storage.setPidKp(pidController.getKp());
    storage.setPidKi(pidController.getKi());
    storage.setPidKd(pidController.getKd());
    
    // Kuluçka durumu
    storage.setIncubationRunning(incubation.isIncubationRunning());
    if (incubation.isIncubationRunning()) {
        storage.setStartTime(incubation.getStartTime());
    }
    
    // Ayarları EEPROM'a kaydet
    storage.saveSettings();
    
    feedWatchdog(); // Ayarları kaydettikten sonra watchdog besleme
}