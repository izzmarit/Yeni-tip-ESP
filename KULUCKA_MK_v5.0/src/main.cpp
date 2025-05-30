/**
 * @file main.cpp
 * @brief KULUÇKA MK v5.0 ana uygulama dosyası (İyileştirilmiş versiyon)
 * @version 1.3
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

// Joystick durum kontrolü için
bool joystickNeedsReset = false;
unsigned long joystickResetTime = 0;
const unsigned long JOYSTICK_RESET_DELAY = 300; // 300ms bekleme

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

    // Tüm alarmları kapat DENEMEK İÇİN ALARM İPTALİ
    alarmManager.disableAlarm(true);
    
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
        
        // Joystick resetleme kontrolü - KALDIRILDI
        handleJoystick();
    }
    
    // Ekranı güncelle - SADECE uygun modda ise
    if (currentMillis - lastDisplayUpdateTime >= DISPLAY_REFRESH_DELAY) {
        lastDisplayUpdateTime = currentMillis;
        
        // Ana ekran modunda ise güncelle
        if (display.getCurrentMode() == DISPLAY_MAIN) {
            updateDisplay();
        }
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
    
    // MENÜ TIMEOUT KALDIRILDI - Artık otomatik ana ekrana dönmez!
    
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
    joystick.update();
    JoystickDirection direction = joystick.readDirection();
    
    if (direction != JOYSTICK_NONE) {
        Serial.print("Joystick: ");
        Serial.println(direction);
        
        menuManager.updateInteractionTime();
        MenuState currentState = menuManager.getCurrentState();
        
        // Ana ekranda sadece sağ yön menüye giriş yapabilir
        if (currentState == MENU_NONE) {
            if (direction == JOYSTICK_RIGHT) {
                menuManager.setCurrentState(MENU_MAIN);
                
                // Ana menü ekranını göster
                std::vector<String> items = menuManager.getMenuItems();
                if (!items.empty()) {
                    display.showMenu(
                        items.data(),
                        items.size(),
                        menuManager.getSelectedIndex()
                    );
                }
            }
            return;
        }
        
        // Menü navigasyonu
        menuManager.update(direction);
        MenuState newState = menuManager.getCurrentState();
        
        // Menü işlemleri
        if (direction == JOYSTICK_PRESS || direction == JOYSTICK_RIGHT) {
            handleMenuActions(direction);
        }
        
        // Ekran güncellemeleri
        if (newState == MENU_NONE) {
            display.setupMainScreen();
        } else if (menuManager.isInTimeAdjustScreen()) {
            display.showTimeAdjustScreen(
                menuManager.getAdjustTitle(),
                menuManager.getTimeString(),
                menuManager.getTimeField()
            );
        } else if (menuManager.isInDateAdjustScreen()) {
            display.showDateAdjustScreen(
                menuManager.getAdjustTitle(),
                menuManager.getDateString(),
                menuManager.getDateField()
            );
        } else if (menuManager.isInValueAdjustScreen()) {
            display.showValueAdjustScreen(
                menuManager.getAdjustTitle(),
                String(menuManager.getAdjustedValue()),
                menuManager.getAdjustUnit()
            );
        } else if (menuManager.isInMenu()) {
            std::vector<String> items = menuManager.getMenuItems();
            if (!items.empty()) {
                display.showMenu(
                    items.data(),
                    items.size(),
                    menuManager.getSelectedIndex()
                );
            }
        }
        
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
    // Mevcut duruma göre ekranı güncelle
    if (menuManager.isInHomeScreen()) {
        // Ana ekran güncellemesi
        display.updateMainScreen(
            sensors.readTemperature(),
            pidController.getSetpoint(),
            sensors.readHumidity(),
            hysteresisController.getSetpoint(),
            relays.getMotorWaitTimeLeft(),      // Kalan bekleme süresi (dakika)
            relays.getMotorRunTimeLeft(),       // Kalan çalışma süresi (saniye)
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
    MenuState currentState = menuManager.getCurrentState();
    
    // Sağa çekme ile değer ayarlama ekranlarına giriş
    if (direction == JOYSTICK_RIGHT) {
        switch (currentState) {
            case MENU_TEMPERATURE:
                menuManager.showValueAdjustScreen("Sicaklik", pidController.getSetpoint(), "C", 20.0, 40.0, 0.1);
                break;
                
            case MENU_HUMIDITY:
                menuManager.showValueAdjustScreen("Nem", hysteresisController.getSetpoint(), "%", 30, 90, 1);
                break;
                
            case MENU_PID_KP:
                menuManager.showValueAdjustScreen("PID Kp", pidController.getKp(), "", 0.0, 100.0, 0.1);
                break;
                
            case MENU_PID_KI:
                menuManager.showValueAdjustScreen("PID Ki", pidController.getKi(), "", 0.0, 100.0, 0.01);
                break;
                
            case MENU_PID_KD:
                menuManager.showValueAdjustScreen("PID Kd", pidController.getKd(), "", 0.0, 100.0, 0.1);
                break;
                
            case MENU_MOTOR_WAIT:
                menuManager.showValueAdjustScreen("Bekleme", storage.getMotorWaitTime(), "dk", 1, 240, 1);
                break;
                
            case MENU_MOTOR_RUN:
                menuManager.showValueAdjustScreen("Calisma", storage.getMotorRunTime(), "sn", 1, 60, 1);
                break;
                
            case MENU_SET_TIME:
                {
                    DateTime now = rtc.getCurrentDateTime();
                    int timeValue = now.hour() * 100 + now.minute();
                    menuManager.showTimeAdjustScreen("Saat Ayarla", timeValue);
                }
                break;
                
            case MENU_SET_DATE:
                {
                    DateTime now = rtc.getCurrentDateTime();
                    long dateValue = now.day() * 1000000L + now.month() * 10000L + now.year();
                    menuManager.showDateAdjustScreen("Tarih Ayarla", dateValue);
                }
                break;
                
            case MENU_CALIBRATION_TEMP:
                menuManager.showValueAdjustScreen("Sicaklik Cal", sensors.getTemperatureCalibration(0), "C", -10.0, 10.0, 0.1);
                break;
                
            case MENU_CALIBRATION_HUMID:
                menuManager.showValueAdjustScreen("Nem Cal", sensors.getHumidityCalibration(0), "%", -20, 20, 1);
                break;
                
            case MENU_ALARM_TEMP_LOW:
                menuManager.showValueAdjustScreen("Dusuk Temp", alarmManager.getTempLowThreshold(), "C", 0.1, 5.0, 0.1);
                break;
                
            case MENU_ALARM_TEMP_HIGH:
                menuManager.showValueAdjustScreen("Yuksek Temp", alarmManager.getTempHighThreshold(), "C", 0.1, 5.0, 0.1);
                break;
                
            case MENU_ALARM_HUMID_LOW:
                menuManager.showValueAdjustScreen("Dusuk Nem", alarmManager.getHumidLowThreshold(), "%", 1, 20, 1);
                break;
                
            case MENU_ALARM_HUMID_HIGH:
                menuManager.showValueAdjustScreen("Yuksek Nem", alarmManager.getHumidHighThreshold(), "%", 1, 20, 1);
                break;
                
            case MENU_MANUAL_DEV_TEMP:
                menuManager.showValueAdjustScreen("Gel Sicaklik", incubation.getParameters().developmentTemp, "C", 20.0, 40.0, 0.1);
                break;
                
            case MENU_MANUAL_HATCH_TEMP:
                menuManager.showValueAdjustScreen("Cik Sicaklik", incubation.getParameters().hatchingTemp, "C", 20.0, 40.0, 0.1);
                break;
                
            case MENU_MANUAL_DEV_HUMID:
                menuManager.showValueAdjustScreen("Gel Nem", incubation.getParameters().developmentHumidity, "%", 30, 90, 1);
                break;
                
            case MENU_MANUAL_HATCH_HUMID:
                menuManager.showValueAdjustScreen("Cik Nem", incubation.getParameters().hatchingHumidity, "%", 30, 90, 1);
                break;
                
            case MENU_MANUAL_DEV_DAYS:
                menuManager.showValueAdjustScreen("Gel Gun", incubation.getParameters().developmentDays, "gun", 1, 60, 1);
                break;
                
            case MENU_MANUAL_HATCH_DAYS:
                menuManager.showValueAdjustScreen("Cik Gun", incubation.getParameters().hatchingDays, "gun", 1, 10, 1);
                break;
                
            case MENU_PID_AUTO_TUNE:
                if (!pidController.isAutoTuneEnabled()) {
                    pidController.setAutoTuneMode(true);
                    display.showConfirmationMessage("Oto Ayar Basladi");
                    menuManager.returnToHome();
                }
                break;
        }
        return;
    }
    
    // Buton basma ile özel işlemler
    if (direction == JOYSTICK_PRESS) {
        switch (currentState) {
            case MENU_INCUBATION_TYPE:
                {
                    int selectedType = menuManager.getSelectedIndex();
                    
                    if (selectedType == 3) {
                        menuManager.setCurrentState(MENU_MANUAL_INCUBATION);
                    } else if (selectedType < 3) {
                        // Kuluçka tipini ayarla ve otomatik başlat
                        incubation.setIncubationType(selectedType);
                        incubation.startIncubation(rtc.getCurrentDateTime());
                        
                        // PID ve Hysteresis hedef değerlerini güncelle
                        pidController.setSetpoint(incubation.getTargetTemperature());
                        hysteresisController.setSetpoint(incubation.getTargetHumidity());
                        
                        // Storage'a kaydet
                        storage.setIncubationType(selectedType);
                        storage.setIncubationRunning(true);
                        storage.setStartTime(rtc.getCurrentDateTime());
                        storage.queueSave();
                        
                        display.showConfirmationMessage("Kulucka Basladi");
                        menuManager.returnToHome();
                    }
                }
                break;
                
            case MENU_MANUAL_START:
                incubation.setIncubationType(INCUBATION_MANUAL);
                incubation.startIncubation(rtc.getCurrentDateTime());
                
                // PID ve Hysteresis hedef değerlerini güncelle
                pidController.setSetpoint(incubation.getTargetTemperature());
                hysteresisController.setSetpoint(incubation.getTargetHumidity());
                
                storage.setIncubationRunning(true);
                storage.setStartTime(rtc.getCurrentDateTime());
                storage.queueSave();
                display.showConfirmationMessage("Manuel Kulucka Basladi");
                menuManager.returnToHome();
                break;
                
            case MENU_ADJUST_VALUE:
            case MENU_SET_TIME:
            case MENU_SET_DATE:
                handleValueAdjustment(direction);
                break;
        }
    }
}

void handleValueAdjustment(JoystickDirection direction) {
    if (direction == JOYSTICK_PRESS) {
        MenuState currentState = menuManager.getCurrentState();
        
        // Saat ayarlama işlemi
        if (currentState == MENU_SET_TIME) {
            int timeValue = menuManager.getAdjustedTimeValue();
            int hour = timeValue / 100;
            int minute = timeValue % 100;
            
            if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
                DateTime now = rtc.getCurrentDateTime();
                rtc.setDateTime(hour, minute, now.day(), now.month(), now.year());
                display.showConfirmationMessage("Saat Kaydedildi");
            } else {
                display.showConfirmationMessage("Gecersiz Saat!");
                return;
            }
            
            menuManager.setCurrentState(MENU_TIME_DATE);
            return;
        }
        
        // Tarih ayarlama işlemi
        if (currentState == MENU_SET_DATE) {
            long dateValue = menuManager.getAdjustedDateValue();
            int day = (int)(dateValue / 1000000);
            int month = (int)((dateValue / 10000) % 100);
            int year = (int)(dateValue % 10000);
            
            if (day >= 1 && day <= 31 && month >= 1 && month <= 12 && year >= 2025 && year <= 2050) {
                DateTime now = rtc.getCurrentDateTime();
                rtc.setDateTime(now.hour(), now.minute(), day, month, year);
                display.showConfirmationMessage("Tarih Kaydedildi");
            } else {
                display.showConfirmationMessage("Gecersiz Tarih!");
                return;
            }
            
            menuManager.setCurrentState(MENU_TIME_DATE);
            return;
        }
        
        // Normal değer ayarlama işlemleri
        MenuState prevState = menuManager.getPreviousState();
        float value = menuManager.getAdjustedValue();
        
        switch (prevState) {
            case MENU_TEMPERATURE:
                pidController.setSetpoint(value);
                // Manuel kuluçka tipindeyse incubation parametrelerini de güncelle
                if (incubation.getIncubationType() == INCUBATION_MANUAL) {
                    incubation.setTargetTemperature(value);
                }
                storage.queueSave();
                break;
                
            case MENU_HUMIDITY:
                hysteresisController.setSetpoint(value);
                // Manuel kuluçka tipindeyse incubation parametrelerini de güncelle
                if (incubation.getIncubationType() == INCUBATION_MANUAL) {
                    incubation.setTargetHumidity((uint8_t)value);
                }
                storage.queueSave();
                break;
                
            case MENU_PID_KP:
                pidController.setTunings(value, pidController.getKi(), pidController.getKd());
                storage.setPidKp(value);
                storage.queueSave();
                break;
                
            case MENU_PID_KI:
                pidController.setTunings(pidController.getKp(), value, pidController.getKd());
                storage.setPidKi(value);
                storage.queueSave();
                break;
                
            case MENU_PID_KD:
                pidController.setTunings(pidController.getKp(), pidController.getKi(), value);
                storage.setPidKd(value);
                storage.queueSave();
                break;
                
            case MENU_MOTOR_WAIT:
                // Motor bekleme süresini güncelle ve relays modülünü bilgilendir
                relays.updateMotorTiming(millis(), (uint32_t)value, storage.getMotorRunTime());
                storage.setMotorWaitTime((uint32_t)value);
                storage.queueSave();
                break;
                
            case MENU_MOTOR_RUN:
                // Motor çalışma süresini güncelle ve relays modülünü bilgilendir
                relays.updateMotorTiming(millis(), storage.getMotorWaitTime(), (uint32_t)value);
                storage.setMotorRunTime((uint32_t)value);
                storage.queueSave();
                break;
                
            case MENU_CALIBRATION_TEMP:
                sensors.setTemperatureCalibration(value, sensors.getTemperatureCalibration(1));
                storage.setTempCalibration(0, value);
                storage.queueSave();
                break;
                
            case MENU_CALIBRATION_HUMID:
                sensors.setHumidityCalibration(value, sensors.getHumidityCalibration(1));
                storage.setHumidCalibration(0, value);
                storage.queueSave();
                break;
                
            case MENU_ALARM_TEMP_LOW:
                alarmManager.setTempLowThreshold(value);
                storage.setTempLowAlarm(value);
                storage.queueSave();
                break;
                
            case MENU_ALARM_TEMP_HIGH:
                alarmManager.setTempHighThreshold(value);
                storage.setTempHighAlarm(value);
                storage.queueSave();
                break;
                
            case MENU_ALARM_HUMID_LOW:
                alarmManager.setHumidLowThreshold(value);
                storage.setHumidLowAlarm(value);
                storage.queueSave();
                break;
                
            case MENU_ALARM_HUMID_HIGH:
                alarmManager.setHumidHighThreshold(value);
                storage.setHumidHighAlarm(value);
                storage.queueSave();
                break;
                
            case MENU_MANUAL_DEV_TEMP:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(
                        value,
                        params.hatchingTemp,
                        params.developmentHumidity,
                        params.hatchingHumidity,
                        params.developmentDays,
                        params.hatchingDays
                    );
                    storage.setManualDevTemp(value);
                    storage.queueSave();
                }
                break;
                
            case MENU_MANUAL_HATCH_TEMP:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(
                        params.developmentTemp,
                        value,
                        params.developmentHumidity,
                        params.hatchingHumidity,
                        params.developmentDays,
                        params.hatchingDays
                    );
                    storage.setManualHatchTemp(value);
                    storage.queueSave();
                }
                break;
                
            case MENU_MANUAL_DEV_HUMID:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(
                        params.developmentTemp,
                        params.hatchingTemp,
                        (uint8_t)value,
                        params.hatchingHumidity,
                        params.developmentDays,
                        params.hatchingDays
                    );
                    storage.setManualDevHumid((uint8_t)value);
                    storage.queueSave();
                }
                break;
                
            case MENU_MANUAL_HATCH_HUMID:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(
                        params.developmentTemp,
                        params.hatchingTemp,
                        params.developmentHumidity,
                        (uint8_t)value,
                        params.developmentDays,
                        params.hatchingDays
                    );
                    storage.setManualHatchHumid((uint8_t)value);
                    storage.queueSave();
                }
                break;
                
            case MENU_MANUAL_DEV_DAYS:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(
                        params.developmentTemp,
                        params.hatchingTemp,
                        params.developmentHumidity,
                        params.hatchingHumidity,
                        (uint8_t)value,
                        params.hatchingDays
                    );
                    storage.setManualDevDays((uint8_t)value);
                    storage.queueSave();
                }
                break;
                
            case MENU_MANUAL_HATCH_DAYS:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(
                        params.developmentTemp,
                        params.hatchingTemp,
                        params.developmentHumidity,
                        params.hatchingHumidity,
                        params.developmentDays,
                        (uint8_t)value
                    );
                    storage.setManualHatchDays((uint8_t)value);
                    storage.queueSave();
                }
                break;
                
            default:
                break;
        }
        
        display.showConfirmationMessage("Kaydedildi");
        
        // Manuel kuluçka ayarlarında alt menüde kal, diğer durumlarda ana menüye dön
        if (prevState >= MENU_MANUAL_DEV_TEMP && prevState <= MENU_MANUAL_HATCH_DAYS) {
            menuManager.setCurrentState(MENU_MANUAL_INCUBATION);
        } else {
            menuManager.setCurrentState(MENU_MAIN);
        }
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
    watchdogManager.beginLongOperation();
    
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
    
    watchdogManager.feed();
    
    // PID parametrelerini ayarla
    pidController.setTunings(
        storage.getPidKp(),
        storage.getPidKi(),
        storage.getPidKd()
    );
    
    // Motor ayarlarını yükle ve relays modülünü güncelle
    relays.updateMotorTiming(
        millis(),
        storage.getMotorWaitTime(),
        storage.getMotorRunTime()
    );
    
    // Kuluçka devam ediyor mu kontrolü
    if (storage.isIncubationRunning()) {
        incubation.startIncubation(storage.getStartTime());
    }
    
    // Hedef değerleri ayarla
    pidController.setSetpoint(incubation.getTargetTemperature());
    hysteresisController.setSetpoint(incubation.getTargetHumidity());
    
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
    
    watchdogManager.endLongOperation();
    watchdogManager.feed();
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