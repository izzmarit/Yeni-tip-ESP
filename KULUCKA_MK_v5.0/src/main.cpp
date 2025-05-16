/**
 * @file main.cpp
 * @brief KULUÇKA MK v5.0 ana uygulama dosyası (İyileştirilmiş versiyon)
 * @version 1.2
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
#include "watchdog_manager.h"
#include "pid_auto_tune.h"

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
WatchdogManager watchdogManager;

// Zaman kontrolü değişkenleri
unsigned long lastSensorReadTime = 0;
unsigned long lastDisplayUpdateTime = 0;
unsigned long lastJoystickReadTime = 0;
unsigned long lastMenuTimeout = 0;
unsigned long lastStorageCheckTime = 0;
unsigned long lastWatchdogFeedTime = 0;

// Menü zaman aşımı (ms) - 30 saniye
const unsigned long MENU_TIMEOUT = 30000;

// Watchdog besleme aralığı (ms) - 5 saniye
const unsigned long WATCHDOG_FEED_INTERVAL = 5000;

// Fonksiyon prototipleri
void initializeModules();
void handleJoystick();
void updateSensors();
void updateDisplay();
void updateRelays();
void updateAlarm();
void checkStorageQueue();
void handleMenuActions(JoystickDirection direction);
void loadSettingsFromStorage();
void saveSettingsToStorage();
void handleValueAdjustment(JoystickDirection direction);
void handlePIDAutoTune();
void feedWatchdogIfNeeded();

void setup() {
    // Seri port başlatma
    Serial.begin(115200);
    Serial.println("KULUCKA MK v5.0 Baslatiliyor...");
    
    // Watchdog timer'ı başlat
    watchdogManager.begin();
    lastWatchdogFeedTime = millis();
    
    // Tüm modülleri başlat
    initializeModules();
    
    // Kaydedilmiş ayarları yükle
    loadSettingsFromStorage();
    
    // Açılış ekranını göster
    display.showSplashScreen();
    feedWatchdogIfNeeded(); // Açılış ekranı gösterilirken watchdog besleme
    
    // Ana ekranı ayarla
    display.setupMainScreen();
    
    Serial.println("KULUCKA MK v5.0 Hazir!");
    feedWatchdogIfNeeded();
}

void loop() {
    // Mevcut zaman
    unsigned long currentMillis = millis();
    
    // Düzenli watchdog beslemesi (5 saniyede bir)
    feedWatchdogIfNeeded();
    
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
    
    // Bekleyen EEPROM değişikliklerini kontrol et
    if (currentMillis - lastStorageCheckTime >= 10000) { // 10 saniyede bir kontrol
        lastStorageCheckTime = currentMillis;
        checkStorageQueue();
    }
    
    // Menü zaman aşımını kontrol et
    if (!menuManager.isInHomeScreen() && 
        (currentMillis - menuManager.getLastInteractionTime() >= MENU_TIMEOUT)) {
        menuManager.returnToHome();
    }
    
    // WiFi isteklerini işle
    wifiManager.handleRequests();
    
    // PID Otomatik Ayarlama durumunu kontrol et
    if (pidController.isAutoTuneEnabled()) {
        handlePIDAutoTune();
    }
}

void feedWatchdogIfNeeded() {
    unsigned long currentMillis = millis();
    
    // Sadece belirli zaman aralıklarında watchdog besle (gereksiz beslemelerden kaçın)
    if (currentMillis - lastWatchdogFeedTime >= WATCHDOG_FEED_INTERVAL) {
        watchdogManager.feed();
        lastWatchdogFeedTime = currentMillis;
    }
}

void initializeModules() {
    // Her modülü başlat ve hata kontrolünü yap
    feedWatchdogIfNeeded();
    
    // Ekran modülü
    if (!display.begin()) {
        Serial.println("Ekran başlatma hatası!");
    }
    feedWatchdogIfNeeded();
    
    // Sensör modülü
    if (!sensors.begin()) {
        Serial.println("Sensör başlatma hatası! En az bir sensör çalışmalı.");
    }
    feedWatchdogIfNeeded();
    
    // RTC modülü
    if (!rtc.begin()) {
        Serial.println("RTC başlatma hatası!");
    }
    feedWatchdogIfNeeded();
    
    // Joystick modülü
    if (!joystick.begin()) {
        Serial.println("Joystick başlatma hatası!");
    }
    feedWatchdogIfNeeded();
    
    // Röle modülü
    if (!relays.begin()) {
        Serial.println("Röle başlatma hatası!");
    }
    feedWatchdogIfNeeded();
    
    // Kuluçka modülü
    if (!incubation.begin()) {
        Serial.println("Kuluçka kontrolü başlatma hatası!");
    }
    feedWatchdogIfNeeded();
    
    // PID kontrolü
    if (!pidController.begin()) {
        Serial.println("PID kontrolü başlatma hatası!");
    }
    feedWatchdogIfNeeded();
    
    // Histerezis kontrolü
    if (!hysteresisController.begin()) {
        Serial.println("Histerezis kontrolü başlatma hatası!");
    }
    feedWatchdogIfNeeded();
    
    // Menü yönetimi
    if (!menuManager.begin()) {
        Serial.println("Menü yönetimi başlatma hatası!");
    }
    feedWatchdogIfNeeded();
    
    // Saklama yönetimi
    if (!storage.begin()) {
        Serial.println("Saklama yönetimi başlatma hatası!");
    }
    feedWatchdogIfNeeded();
    
    // WiFi yönetimi - uzun süren işlem
    watchdogManager.beginLongOperation();
    if (!wifiManager.beginAP()) {
        Serial.println("WiFi başlatma hatası!");
    } else {
        wifiManager.startServer();
    }
    watchdogManager.endLongOperation();
    feedWatchdogIfNeeded();
    
    // Alarm yönetimi
    if (!alarmManager.begin()) {
        Serial.println("Alarm yönetimi başlatma hatası!");
    }
    feedWatchdogIfNeeded();
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
        // Kullanıcı etkileşimi olduğunda watchdog'u besle
        feedWatchdogIfNeeded();
    }
}

void updateSensors() {
    // Sensör verilerini okumadan önce watchdog beslemesi
    bool needWatchdogFeed = false;
    
    // Sıcaklık ve nem değerlerini oku
    float temp = sensors.readTemperature();
    float humid = sensors.readHumidity();
    
    // Eğer sensör hata sayısı yüksekse watchdog beslemeyi zorla
    if (sensors.getI2CErrorCount() > 5) {
        needWatchdogFeed = true;
    }
    
    // PID kontrolü için sıcaklık değerini kullan
    pidController.compute(temp);
    
    // Histerezis kontrolü için nem değerini kullan
    hysteresisController.compute(humid);
    
    // Kuluçka durumunu güncelle
    incubation.update(rtc.getCurrentDateTime());
    
    // RTC hata sayısı yüksekse watchdog beslemeyi zorla
    if (rtc.getRTCErrorCount() > 2) {
        needWatchdogFeed = true;
    }
    
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
    
    // Alarmları kontrol et
    alarmManager.checkAlarms(
        temp, 
        incubation.getTargetTemperature(), 
        humid, 
        incubation.getTargetHumidity(), 
        relays.getMotorState(), 
        true,  // Motor zamanı doğru mu kontrolü yapılmalı
        sensors.areSensorsWorking()
    );
    
    // Gerekirse watchdog besle
    if (needWatchdogFeed) {
        feedWatchdogIfNeeded();
    }
}

void updateDisplay() {
    // Ekran güncellemesi başlangıcında watchdog beslemeye gerek yok
    
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
    
    // PID Otomatik Ayarlama ekranı
    if (pidController.isAutoTuneEnabled()) {
        // İlerleme çubuğu göster
        display.showProgressBar(
            20, SCREEN_HEIGHT / 2, 
            SCREEN_WIDTH - 40, 20, 
            COLOR_HIGHLIGHT, 
            pidController.getAutoTuneProgress()
        );
        
        if (pidController.isAutoTuneFinished()) {
            display.showConfirmationMessage("Otomatik Ayarlama Tamamlandi");
            pidController.setAutoTuneMode(false);
            saveSettingsToStorage();
        }
    }
    
    // Alarm varsa alarm mesajını göster
    if (alarmManager.getCurrentAlarm() != ALARM_NONE) {
        display.showAlarmMessage(
            alarmManager.getAlarmMessage(),
            "Kontrol Et!"
        );
    }
    
    // Ekran güncellemesi sonrası watchdog besleme gerekmez
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

void checkStorageQueue() {
    // Saklama işlemi uzun sürebilir, önce watchdog'u besle
    feedWatchdogIfNeeded();
    
    // Bekleyen EEPROM değişikliklerini işle
    storage.processQueue();
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
                    storage.queueSave(); // Gecikmeli kayıt
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
                
            case MENU_PID_MODE:
                // PID modu seçimi
                if (menuManager.getSelectedIndex() == 0) {
                    // Manuel PID - PID ayar menüsüne git
                    pidController.setAutoTuneMode(false);
                    // menuManager.showMenu() yerine
                    menuManager.setCurrentState(MENU_PID);
                    menuManager.setSelectedIndex(0);
                } else if (menuManager.getSelectedIndex() == 1) {
                    // Otomatik Ayarlama - Otomatik ayarlama ekranına git
                    if (!pidController.isAutoTuneEnabled()) {
                        display.showConfirmationMessage("Otomatik Ayarlama Baslatiliyor");
                        pidController.setAutoTuneMode(true);
                    }
                }
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
                    storage.queueSave(); // Gecikmeli kayıt
                } else if (menuManager.getCurrentState() == MENU_HUMIDITY) {
                    // Değişikliği uygula
                    // TODO: Kuluçka tipine göre nem değişimi
                    
                    display.showConfirmationMessage("Deger kaydedildi");
                    storage.queueSave(); // Gecikmeli kayıt
                }
                // Diğer değer kayıtları için de benzer işlemler yapılmalı
                
                break;
                
            default:
                break;
        }
    }
}

void handleValueAdjustment(JoystickDirection direction) {
    // Ayarlanan değeri güncelle
    menuManager.update(direction);
    
    // Değeri göster
    display.showValueAdjustScreen(
        "Deger Ayarlama",
        String(menuManager.getAdjustedValue()),
        ""
    );
    
    // Buton basıldığında değer değişimini kaydet
    if (joystick.wasButtonPressed()) {
        // TODO: Değeri ilgili modüle kaydet (hangi değer ayarlandığına bağlı olarak)
        display.showConfirmationMessage("Deger kaydedildi");
        storage.queueSave(); // Gecikmeli kayıt
    }
}

void handlePIDAutoTune() {
    // PID Otomatik Ayarlama durumunu kontrol et ve güvenlik önlemlerini uygula
    if (pidController.isAutoTuneEnabled()) {
        // Watchdog'u daha sık besle, çünkü kritik bir işlem
        feedWatchdogIfNeeded();
        
        // Otomatik ayarlama tamamlandı mı?
        if (pidController.isAutoTuneFinished()) {
            // Otomatik ayarlama tamamlandı, değerleri kaydet
            storage.queueSave();
        }
        
        // PID otomatik ayarlama sırasında güvenlik önlemleri
        float currentTemp = sensors.readTemperature();
        
        // Sensörler çalışmıyorsa otomatik ayarlamayı iptal et
        if (!sensors.areSensorsWorking()) {
            Serial.println("Otomatik Ayarlama: Sensör hatası nedeniyle iptal edildi!");
            pidController.setAutoTuneMode(false);
            display.showConfirmationMessage("Oto Ayar Iptal: Sensor Hatasi");
        }
    }
}

void loadSettingsFromStorage() {
    watchdogManager.beginLongOperation(); // Uzun işlem için watchdog süresini artır
    
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
    
    // Watchdog besleme - uzun işlem sırasında
    watchdogManager.feed();
    
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
    
    // Watchdog besleme - uzun işlem sırasında
    watchdogManager.feed();
    
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
    
    watchdogManager.endLongOperation(); // Normal watchdog süresine geri dön
    watchdogManager.feed(); // İşlem bitince watchdog besle
}

void saveSettingsToStorage() {
    watchdogManager.beginLongOperation(); // Uzun işlem için watchdog süresini artır
    
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
    
    // Watchdog besleme - uzun işlem sırasında
    watchdogManager.feed();
    
    // PID parametreleri
    storage.setPidKp(pidController.getKp());
    storage.setPidKi(pidController.getKi());
    storage.setPidKd(pidController.getKd());
    
    // Kuluçka durumu
    storage.setIncubationRunning(incubation.isIncubationRunning());
    if (incubation.isIncubationRunning()) {
        storage.setStartTime(incubation.getStartTime());
    }
    
    // Ayarları anında EEPROM'a kaydet (kritik durumlarda)
    storage.saveStateNow();
    
    watchdogManager.endLongOperation(); // Normal watchdog süresine geri dön
    watchdogManager.feed(); // İşlem bitince watchdog besle
}