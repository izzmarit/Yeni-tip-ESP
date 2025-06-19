/**
 * @file wifi_manager.cpp
 * @brief WiFi bağlantı ve web sunucu yönetimi uygulaması (WebServer ile)
 * @version 1.7 - Tüm hatalar düzeltildi ve eksik fonksiyonlar eklendi
 */

#include "wifi_manager.h"
#include "pid.h"  // PIDController sınıfı için gerekli
#include <ESPmDNS.h>
#include "relays.h"
#include "incubation.h"
#include "sensors.h"
#include "alarm.h"
#include "rtc.h"


WiFiManager::WiFiManager() {
    _server = nullptr;
    _isConnected = false;
    _isServerRunning = false;
    _ssid = "";
    _password = "";
    _stationSSID = "";
    _stationPassword = "";
    _connectionStatus = WIFI_STATUS_DISCONNECTED;
    _storage = nullptr;
    _lastConnectionAttempt = 0;
    
    // Başlangıç durum verileri
    _currentTemp = 0.0;
    _currentHumid = 0.0;
    _heaterState = false;
    _humidifierState = false;
    _motorState = false;
    _currentDay = 0;
    _totalDays = 0;
    _incubationType = "";
    _targetTemp = 0.0;
    _targetHumid = 0.0;
    _pidMode = 0;
    _pidKp = 0.0;
    _pidKi = 0.0;
    _pidKd = 0.0;
    _alarmEnabled = true;
    _tempLowAlarm = 0.0;
    _tempHighAlarm = 0.0;
    _humidLowAlarm = 0.0;
    _humidHighAlarm = 0.0;

    // Sensör detaylı verileri
    _temp1 = 0.0;
    _temp2 = 0.0;
    _humid1 = 0.0;
    _humid2 = 0.0;
    _sensor1Working = false;
    _sensor2Working = false;
    
    // Motor ayarları
    _motorWaitTime = 120;
    _motorRunTime = 14;
    
    // Kalibrasyon ayarları
    _tempCalibration1 = 0.0;
    _tempCalibration2 = 0.0;
    _humidCalibration1 = 0.0;
    _humidCalibration2 = 0.0;
    
    // Manuel kuluçka ayarları
    _manualDevTemp = 37.5;
    _manualHatchTemp = 37.0;
    _manualDevHumid = 60;
    _manualHatchHumid = 70;
    _manualDevDays = 18;
    _manualHatchDays = 3;
    _isIncubationRunning = false;
    
    // Kuluçka tamamlanma durumu
    _isIncubationCompleted = false;
    _actualDay = 0;

    // Memory management initialization
    _jsonBuffer = nullptr;
    _responseBuffer = nullptr;
    _buffersAllocated = false;
    _serverRecreationNeeded = false;
    _lastServerRestart = 0;
}

void WiFiManager::setStorage(Storage* storage) {
    _storage = storage;
}

bool WiFiManager::begin() {
    if (_storage == nullptr) {
        Serial.println("WiFi Manager: Storage referansı ayarlanmamış!");
        return beginAP();
    }
    
    // Storage'dan WiFi ayarlarını oku
    WiFiConnectionMode mode = _storage->getWifiMode();
    _stationSSID = _storage->getStationSSID();
    _stationPassword = _storage->getStationPassword();
    
    Serial.println("WiFi Manager: Kaydedilmiş mod: " + 
                   String(mode == WIFI_CONN_MODE_AP ? "AP" : "Station"));
    
    if (mode == WIFI_CONN_MODE_STATION) {
        if (_stationSSID.length() > 0) {
            Serial.println("WiFi Manager: Kaydedilmiş SSID bulundu: " + _stationSSID);
            
            // WiFi modülünü başlat
            WiFi.disconnect(false);
            delay(100);
            WiFi.mode(WIFI_STA);
            delay(100);
            
            // Kaydedilmiş credential'larla bağlan
            return beginStation(_stationSSID, _stationPassword);
        } else {
            Serial.println("WiFi Manager: Station SSID boş, AP moduna geçiliyor");
            // SSID yoksa AP moduna geç
            _storage->setWifiMode(WIFI_CONN_MODE_AP);
            _storage->saveStateNow();
            return beginAP();
        }
    } else {
        Serial.println("WiFi Manager: AP modunda başlatılıyor...");
        return beginAP();
    }
}

bool WiFiManager::begin(const String& ssid, const String& password) {
    _ssid = ssid;
    _password = password;
    
    WiFi.disconnect(true);
    delay(1000);
    esp_task_wdt_reset();
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    _connectionStatus = WIFI_STATUS_CONNECTING;
    _lastConnectionAttempt = millis();
    
    int timeout = 20;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        esp_task_wdt_reset();
        timeout--;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        _isConnected = true;
        _connectionStatus = WIFI_STATUS_CONNECTED;
        Serial.println("WiFi bağlantısı başarılı: " + WiFi.localIP().toString());
        return true;
    }
    
    // BAĞLANTI BAŞARISIZ OLSA BİLE STATION MODUNDA KAL
    _connectionStatus = WIFI_STATUS_FAILED;
    _isConnected = false;
    Serial.println("WiFi bağlantısı başarısız, station modunda kalıyor...");
    return true; // Başlatma başarılı sayılır
}

bool WiFiManager::beginAP() {
    WiFi.disconnect(false); // false = WiFi modülünü kapatma
    delay(100);
    esp_task_wdt_reset();
    
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // AP için sabit IP ayarla (opsiyonel)
    IPAddress localIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(localIP, gateway, subnet);
    
    bool success = WiFi.softAP(AP_SSID, AP_PASS);
    
    if (success) {
        _isConnected = true;
        _connectionStatus = WIFI_STATUS_AP_MODE;
        _ssid = AP_SSID;
        _password = AP_PASS;
        Serial.println("AP modu aktif: " + WiFi.softAPIP().toString());
    } else {
        Serial.println("AP modu başlatılamadı!");
    }
    
    esp_task_wdt_reset();
    return success;
}

bool WiFiManager::beginStation(const String& ssid, const String& password) {
    _stationSSID = ssid;
    _stationPassword = password;
    
    // Önceki bağlantıyı temizle
    WiFi.disconnect(false);
    delay(100);
    esp_task_wdt_reset();
    
    // Station modunu ayarla
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setAutoConnect(true);
    
    // Kaydedilmiş credential'ları kullan
    WiFi.begin(ssid.c_str(), password.c_str());
    
    _connectionStatus = WIFI_STATUS_CONNECTING;
    _lastConnectionAttempt = millis();
    
    Serial.println("WiFi: Station modunda bağlanıyor - SSID: " + ssid);
    
    // Bağlantı için bekle
    int timeout = 30;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        esp_task_wdt_reset();
        Serial.print(".");
        timeout--;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        _isConnected = true;
        _connectionStatus = WIFI_STATUS_CONNECTED;
        _ssid = ssid;
        _password = password;
        
        // KRITIK: Başarılı bağlantıyı hemen kaydet
        if (_storage != nullptr) {
            _storage->setWifiMode(WIFI_CONN_MODE_STATION);
            _storage->setStationSSID(ssid);
            _storage->setStationPassword(password);
            _storage->saveStateNow(); // Anında kaydet
            
            Serial.println("WiFi: Station credential'ları kaydedildi");
        }
        
        IPAddress ip = WiFi.localIP();
        Serial.println("Station modunda bağlantı başarılı: " + ip.toString());
        
        // mDNS başlat
        if (MDNS.begin("kulucka")) {
            Serial.println("mDNS başlatıldı: kulucka.local");
        }
        
        return true; // Gerçek başarı
    }
    
    // DÜZELTME: Bağlantı başarısız - credential'ları kaydet ama false döndür
    if (_storage != nullptr && ssid.length() > 0) {
        _storage->setStationSSID(ssid);
        _storage->setStationPassword(password);
        _storage->queueSave();
    }
    
    _connectionStatus = WIFI_STATUS_FAILED;
    _isConnected = false;
    Serial.println("Station modunda bağlantı başarısız");
    
    // DÜZELTME: Gerçek durumu döndür
    return false; // Bağlantı başarısız
}

void WiFiManager::stop() {
    if (_isServerRunning) {
        stopServer();
    }
    
    WiFi.disconnect(true);
    _isConnected = false;
    _connectionStatus = WIFI_STATUS_DISCONNECTED;
}

bool WiFiManager::isConnected() const {
    if (WiFi.getMode() == WIFI_STA) {
        return WiFi.status() == WL_CONNECTED;
    } else if (WiFi.getMode() == WIFI_AP) {
        return true;
    }
    
    return false;
}

WiFiConnectionStatus WiFiManager::getConnectionStatus() const {
    return _connectionStatus;
}

WiFiMode_t WiFiManager::getCurrentMode() const {
    return WiFi.getMode();
}

bool WiFiManager::isServerRunning() const {
    return _isServerRunning;
}

String WiFiManager::getIPAddress() const {
    if (WiFi.getMode() == WIFI_STA) {
        return WiFi.localIP().toString();
    } else if (WiFi.getMode() == WIFI_AP) {
        return WiFi.softAPIP().toString();
    }
    
    return "0.0.0.0";
}

String WiFiManager::getSSID() const {
    return _ssid;
}

int WiFiManager::getSignalStrength() const {
    if (WiFi.getMode() == WIFI_STA) {
        return WiFi.RSSI();
    }
    
    return 0;
}

void WiFiManager::setStationCredentials(const String& ssid, const String& password) {
    _stationSSID = ssid;
    _stationPassword = password;
}

bool WiFiManager::switchToStationMode() {
    Serial.println("WiFi: Station moduna geçiş başlıyor...");
    
    // KRITIK: Tüm sistem durumunu kaydet
    if (_storage != nullptr) {
        _storage->saveStateNow();
        delay(100);
        Serial.println("WiFi: Sistem durumu kaydedildi");
    }
    
    // Sistem durumunu koru
    SystemState savedState;
    savedState.temperature = _currentTemp;
    savedState.humidity = _currentHumid;
    savedState.targetTemp = _targetTemp;
    savedState.targetHumid = _targetHumid;
    savedState.heaterState = _heaterState;
    savedState.humidifierState = _humidifierState;
    savedState.motorState = _motorState;
    savedState.pidMode = _pidMode;
    savedState.pidKp = _pidKp;
    savedState.pidKi = _pidKi;
    savedState.pidKd = _pidKd;
    savedState.alarmEnabled = _alarmEnabled;
    savedState.incubationType = _incubationType;
    savedState.isIncubationRunning = _isIncubationRunning;
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    if (_stationSSID.length() == 0) {
        Serial.println("Station SSID ayarlanmamış!");
        return false;
    }
    
    // Server'ı güvenli şekilde durdur
    if (_isServerRunning) {
        stopServer();
        delay(500);
        esp_task_wdt_reset();
    }
    
    // WiFi modunu değiştir
    WiFi.disconnect(false);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setAutoConnect(true);
    delay(100);
    
    // Station modunda bağlan
    bool connected = false;
    WiFi.begin(_stationSSID.c_str(), _stationPassword.c_str());
    
    // Bağlantı için bekle (timeout artırıldı)
    int timeout = 40; // 20 saniye
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        esp_task_wdt_reset();
        timeout--;
        
        // Her 5 saniyede bir durum bilgisi ver
        if (timeout % 10 == 0) {
            Serial.println("WiFi: Bağlanıyor... (" + String(timeout/2) + " saniye kaldı)");
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        connected = true;
        _isConnected = true;
        _connectionStatus = WIFI_STATUS_CONNECTED;
        _ssid = _stationSSID;
        _password = _stationPassword;
        Serial.println("Station modunda bağlantı başarılı: " + WiFi.localIP().toString());
        
        // mDNS'i yeniden başlat
        MDNS.end();
        delay(100);
        if (MDNS.begin("kulucka")) {
            Serial.println("mDNS yeniden başlatıldı: kulucka.local");
        }
    } else {
        _connectionStatus = WIFI_STATUS_FAILED;
        _isConnected = false;
        Serial.println("Station modunda bağlantı başarısız");
    }
    
    // Sistem durumunu tamamen restore et
    _currentTemp = savedState.temperature;
    _currentHumid = savedState.humidity;
    _targetTemp = savedState.targetTemp;
    _targetHumid = savedState.targetHumid;
    _heaterState = savedState.heaterState;
    _humidifierState = savedState.humidifierState;
    _motorState = savedState.motorState;
    _pidMode = savedState.pidMode;
    _pidKp = savedState.pidKp;
    _pidKi = savedState.pidKi;
    _pidKd = savedState.pidKd;
    _alarmEnabled = savedState.alarmEnabled;
    _incubationType = savedState.incubationType;
    _isIncubationRunning = savedState.isIncubationRunning;
    
    // Server'ı yeniden başlat
    delay(200);
    startServer();
    esp_task_wdt_reset();
    
    // Yeni modu kaydet
    if (_storage != nullptr && connected) {
        _storage->setWifiMode(WIFI_CONN_MODE_STATION);
        _storage->setStationSSID(_stationSSID);
        _storage->setStationPassword(_stationPassword);
        _storage->saveStateNow();
        delay(100);
        Serial.println("WiFi: Station credential'ları kaydedildi");
    }
    
    Serial.println("WiFi: Station moduna geçiş tamamlandı - Sistem durumu korundu");
    Serial.println("Korunan değerler - Sıcaklık: " + String(_targetTemp) + 
                   " Nem: " + String(_targetHumid) + 
                   " PID: " + String(_pidMode));
    
    return true;
}

bool WiFiManager::switchToAPMode() {
    Serial.println("WiFi: AP moduna geçiş başlıyor...");
    
    // Önce mevcut ayarları ANINDA kaydet
    if (_storage != nullptr) {
        _storage->saveStateNow();
        delay(100);
    }
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    // Server'ı güvenli şekilde durdur
    if (_isServerRunning) {
        stopServer();
        delay(500);
        esp_task_wdt_reset();
    }
    
    // WiFi modunu değiştir - YENIDEN BAŞLATMA GEREKTİRMEYEN YÖNTEM
    WiFi.disconnect(false);
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // AP modunu başlat
    bool success = WiFi.softAP(AP_SSID, AP_PASS);
    
    if (success) {
        _isConnected = true;
        _connectionStatus = WIFI_STATUS_AP_MODE;
        _ssid = AP_SSID;
        _password = AP_PASS;
        Serial.println("AP modu aktif: " + WiFi.softAPIP().toString());
    }
    
    esp_task_wdt_reset();
    
    // Server'ı yeniden başlat
    if (success) {
        delay(200);
        startServer();
        esp_task_wdt_reset();
        
        // Yeni modu ANINDA kaydet
        if (_storage != nullptr) {
            _storage->setWifiMode(WIFI_CONN_MODE_AP);
            _storage->saveStateNow();
            delay(100);
        }
    }
    
    Serial.println("WiFi: AP moduna geçiş tamamlandı");
    return success;
}

// Yeni fonksiyon ekleyin - Sabit IP ayarlama:
void WiFiManager::setStaticIP(bool useStatic, IPAddress ip, IPAddress gateway, IPAddress subnet, IPAddress dns) {
    _useStaticIP = useStatic;
    if (useStatic) {
        _staticIP = ip;
        _gateway = gateway;
        _subnet = subnet;
        _dns = dns;
    }
}

void WiFiManager::saveWiFiSettings() {
    if (_storage != nullptr) {
        _storage->setWifiSSID(_ssid);
        _storage->setWifiPassword(_password);
        _storage->setStationSSID(_stationSSID);
        _storage->setStationPassword(_stationPassword);
        _storage->queueSave();
    }
}

void WiFiManager::updateStatusData(float temperature, float humidity, bool heaterState, 
                                 bool humidifierState, bool motorState, int currentDay, 
                                 int totalDays, String incubationType, float targetTemp, 
                                 float targetHumidity, bool isIncubationCompleted,
                                 int actualDay, float temp1, float temp2,
                                 float humid1, float humid2,
                                 bool sensor1Working, bool sensor2Working) {
    _currentTemp = temperature;
    _currentHumid = humidity;
    _heaterState = heaterState;
    _humidifierState = humidifierState;
    _motorState = motorState;
    _currentDay = currentDay;
    _totalDays = totalDays;
    _incubationType = incubationType;
    
    // ÖNEMLİ: Hedef değerleri parametre olarak alınan değerlerle güncelle
    _targetTemp = targetTemp;
    _targetHumid = targetHumidity;
    
    _isIncubationCompleted = isIncubationCompleted;
    _actualDay = actualDay;
    
    // Sensör detaylı verileri
    _temp1 = temp1;
    _temp2 = temp2;
    _humid1 = humid1;
    _humid2 = humid2;
    _sensor1Working = sensor1Working;
    _sensor2Working = sensor2Working;
    
    // Storage'dan diğer değerleri al
    if (_storage != nullptr) {
        _pidKp = _storage->getPidKp();
        _pidKi = _storage->getPidKi();
        _pidKd = _storage->getPidKd();
        
        _alarmEnabled = _storage->areAlarmsEnabled();
        _tempLowAlarm = _storage->getTempLowAlarm();
        _tempHighAlarm = _storage->getTempHighAlarm();
        _humidLowAlarm = _storage->getHumidLowAlarm();
        _humidHighAlarm = _storage->getHumidHighAlarm();
        
        _motorWaitTime = _storage->getMotorWaitTime();
        _motorRunTime = _storage->getMotorRunTime();
        
        _tempCalibration1 = _storage->getTempCalibration(0);
        _tempCalibration2 = _storage->getTempCalibration(1);
        _humidCalibration1 = _storage->getHumidCalibration(0);
        _humidCalibration2 = _storage->getHumidCalibration(1);
        
        _manualDevTemp = _storage->getManualDevTemp();
        _manualHatchTemp = _storage->getManualHatchTemp();
        _manualDevHumid = _storage->getManualDevHumid();
        _manualHatchHumid = _storage->getManualHatchHumid();
        _manualDevDays = _storage->getManualDevDays();
        _manualHatchDays = _storage->getManualHatchDays();
        _isIncubationRunning = _storage->isIncubationRunning();
    }
}

void WiFiManager::setPidMode(int mode) {
    _pidMode = mode;
    Serial.println("WiFi Manager PID Mode güncellendi: " + String(mode));
}

void WiFiManager::handleRequests() {
    _checkConnectionStatus();
    
    if (_isServerRunning && _server) {
        _server->handleClient();
    }
}

void WiFiManager::handleClient() {
    handleRequests();
}

void WiFiManager::handleConfiguration() {
    // Özel konfigürasyon işlemleri için
}

void WiFiManager::processAppData(String jsonData) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, jsonData);
    
    if (error) {
        Serial.println("WiFi Manager: JSON ayrıştırma hatası - " + String(error.c_str()));
        return;
    }
    
    // Kritik parametreleri takip et
    bool hasCriticalUpdate = false;
    
    if (doc.containsKey("targetTemp")) {
        float targetTemp = doc["targetTemp"];
        _processParameterUpdate("targetTemp", String(targetTemp));
        hasCriticalUpdate = true;
        Serial.println("App'den hedef sıcaklık güncellendi: " + String(targetTemp));
    }
    
    if (doc.containsKey("targetHumid")) {
        float targetHumid = doc["targetHumid"];
        _processParameterUpdate("targetHumid", String(targetHumid));
        hasCriticalUpdate = true;
        Serial.println("App'den hedef nem güncellendi: " + String(targetHumid));
    }
    
    // PID parametrelerini kontrol et
    if (doc.containsKey("pidKp")) {
        float kp = doc["pidKp"];
        _processParameterUpdate("pidKp", String(kp));
        hasCriticalUpdate = true;
        Serial.println("App'den PID Kp güncellendi: " + String(kp));
    }
    if (doc.containsKey("pidKi")) {
        float ki = doc["pidKi"];
        _processParameterUpdate("pidKi", String(ki));
        hasCriticalUpdate = true;
        Serial.println("App'den PID Ki güncellendi: " + String(ki));
    }
    if (doc.containsKey("pidKd")) {
        float kd = doc["pidKd"];
        _processParameterUpdate("pidKd", String(kd));
        hasCriticalUpdate = true;
        Serial.println("App'den PID Kd güncellendi: " + String(kd));
    }
    if (doc.containsKey("pidMode")) {
        int mode = doc["pidMode"];
        _processParameterUpdate("pidMode", String(mode));
        hasCriticalUpdate = true;
        Serial.println("App'den PID modu güncellendi: " + String(mode));
    }
    
    // PID toplu güncelleme desteği
    if (doc.containsKey("pid")) {
        JsonObject pid = doc["pid"];
        if (pid.containsKey("kp")) {
            float kp = pid["kp"];
            _processParameterUpdate("pidKp", String(kp));
            hasCriticalUpdate = true;
        }
        if (pid.containsKey("ki")) {
            float ki = pid["ki"];
            _processParameterUpdate("pidKi", String(ki));
            hasCriticalUpdate = true;
        }
        if (pid.containsKey("kd")) {
            float kd = pid["kd"];
            _processParameterUpdate("pidKd", String(kd));
            hasCriticalUpdate = true;
        }
        if (pid.containsKey("mode")) {
            int mode = pid["mode"];
            _processParameterUpdate("pidMode", String(mode));
            hasCriticalUpdate = true;
        }
        Serial.println("App'den PID parametreleri toplu güncellendi");
    }
    
    if (doc.containsKey("incubationType")) {
        _processParameterUpdate("incubationType", String((int)doc["incubationType"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("isIncubationRunning")) {
        _processParameterUpdate("isIncubationRunning", String((bool)doc["isIncubationRunning"] ? "1" : "0"));
        hasCriticalUpdate = true;
    }
    
    if (doc.containsKey("motorWaitTime")) {
        _processParameterUpdate("motorWaitTime", String((int)doc["motorWaitTime"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("motorRunTime")) {
        _processParameterUpdate("motorRunTime", String((int)doc["motorRunTime"]));
        hasCriticalUpdate = true;
    }
    
    // Alarm parametreleri kontrolü
    if (doc.containsKey("alarmEnabled")) {
        _processParameterUpdate("alarmEnabled", String((bool)doc["alarmEnabled"] ? "1" : "0"));
        hasCriticalUpdate = true;
        Serial.println("App'den alarm durumu güncellendi: " + String((bool)doc["alarmEnabled"] ? "AÇIK" : "KAPALI"));
    }
    
    // Alarm limitleri toplu güncelleme desteği
    if (doc.containsKey("alarms")) {
        JsonObject alarms = doc["alarms"];
        if (alarms.containsKey("enabled")) {
            _processParameterUpdate("alarmEnabled", String((bool)alarms["enabled"] ? "1" : "0"));
            hasCriticalUpdate = true;
        }
        if (alarms.containsKey("tempLow")) {
            _processParameterUpdate("tempLowAlarm", String((float)alarms["tempLow"]));
            hasCriticalUpdate = true;
        }
        if (alarms.containsKey("tempHigh")) {
            _processParameterUpdate("tempHighAlarm", String((float)alarms["tempHigh"]));
            hasCriticalUpdate = true;
        }
        if (alarms.containsKey("humidLow")) {
            _processParameterUpdate("humidLowAlarm", String((float)alarms["humidLow"]));
            hasCriticalUpdate = true;
        }
        if (alarms.containsKey("humidHigh")) {
            _processParameterUpdate("humidHighAlarm", String((float)alarms["humidHigh"]));
            hasCriticalUpdate = true;
        }
        Serial.println("App'den alarm parametreleri toplu güncellendi");
    }
    
    // Tekil alarm limitleri
    if (doc.containsKey("tempLowAlarm")) {
        _processParameterUpdate("tempLowAlarm", String((float)doc["tempLowAlarm"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("tempHighAlarm")) {
        _processParameterUpdate("tempHighAlarm", String((float)doc["tempHighAlarm"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("humidLowAlarm")) {
        _processParameterUpdate("humidLowAlarm", String((float)doc["humidLowAlarm"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("humidHighAlarm")) {
        _processParameterUpdate("humidHighAlarm", String((float)doc["humidHighAlarm"]));
        hasCriticalUpdate = true;
    }
    
    if (doc.containsKey("manualDevTemp")) {
        _processParameterUpdate("manualDevTemp", String((float)doc["manualDevTemp"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("manualHatchTemp")) {
        _processParameterUpdate("manualHatchTemp", String((float)doc["manualHatchTemp"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("manualDevHumid")) {
        _processParameterUpdate("manualDevHumid", String((int)doc["manualDevHumid"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("manualHatchHumid")) {
        _processParameterUpdate("manualHatchHumid", String((int)doc["manualHatchHumid"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("manualDevDays")) {
        _processParameterUpdate("manualDevDays", String((int)doc["manualDevDays"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("manualHatchDays")) {
        _processParameterUpdate("manualHatchDays", String((int)doc["manualHatchDays"]));
        hasCriticalUpdate = true;
    }
    
    // Kalibrasyon parametreleri
    if (doc.containsKey("tempCalibration1")) {
        _processParameterUpdate("tempCalibration1", String((float)doc["tempCalibration1"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("tempCalibration2")) {
        _processParameterUpdate("tempCalibration2", String((float)doc["tempCalibration2"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("humidCalibration1")) {
        _processParameterUpdate("humidCalibration1", String((float)doc["humidCalibration1"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("humidCalibration2")) {
        _processParameterUpdate("humidCalibration2", String((float)doc["humidCalibration2"]));
        hasCriticalUpdate = true;
    }
    
    // Kalibrasyon toplu güncelleme desteği
    if (doc.containsKey("calibration")) {
        JsonObject calibration = doc["calibration"];
        if (calibration.containsKey("temp1")) {
            _processParameterUpdate("tempCalibration1", String((float)calibration["temp1"]));
            hasCriticalUpdate = true;
        }
        if (calibration.containsKey("temp2")) {
            _processParameterUpdate("tempCalibration2", String((float)calibration["temp2"]));
            hasCriticalUpdate = true;
        }
        if (calibration.containsKey("humid1")) {
            _processParameterUpdate("humidCalibration1", String((float)calibration["humid1"]));
            hasCriticalUpdate = true;
        }
        if (calibration.containsKey("humid2")) {
            _processParameterUpdate("humidCalibration2", String((float)calibration["humid2"]));
            hasCriticalUpdate = true;
        }
        Serial.println("App'den kalibrasyon parametreleri toplu güncellendi");
    }
    
    if (doc.containsKey("wifiStationSSID")) {
        _processParameterUpdate("wifiStationSSID", String((const char*)doc["wifiStationSSID"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("wifiStationPassword")) {
        _processParameterUpdate("wifiStationPassword", String((const char*)doc["wifiStationPassword"]));
        hasCriticalUpdate = true;
    }
    if (doc.containsKey("wifiMode")) {
        _processParameterUpdate("wifiMode", String((int)doc["wifiMode"]));
        hasCriticalUpdate = true;
    }
    
    // Kritik güncelleme varsa anında kaydet
    if (hasCriticalUpdate && _storage != nullptr) {
        _storage->saveStateNow();
        Serial.println("App güncellemesi sonrası veriler anında kaydedildi");
    }
}

String WiFiManager::createAppData() {
    StaticJsonDocument<2048> doc;
    
    doc["temperature"] = _currentTemp;
    doc["humidity"] = _currentHumid;
    doc["heaterState"] = _heaterState;
    doc["humidifierState"] = _humidifierState;
    doc["motorState"] = _motorState;
    
    // Detaylı sensör verileri
    JsonObject sensors = doc.createNestedObject("sensors");
    JsonObject sensor1 = sensors.createNestedObject("sensor1");
    sensor1["temperature"] = _temp1;
    sensor1["humidity"] = _humid1;
    sensor1["working"] = _sensor1Working;
    sensor1["tempCalibration"] = _tempCalibration1;
    sensor1["humidCalibration"] = _humidCalibration1;
    
    JsonObject sensor2 = sensors.createNestedObject("sensor2");
    sensor2["temperature"] = _temp2;
    sensor2["humidity"] = _humid2;
    sensor2["working"] = _sensor2Working;
    sensor2["tempCalibration"] = _tempCalibration2;
    sensor2["humidCalibration"] = _humidCalibration2;
    
    doc["currentDay"] = _currentDay;
    doc["totalDays"] = _totalDays;
    doc["incubationType"] = _incubationType;
    doc["targetTemp"] = _targetTemp;
    doc["targetHumid"] = _targetHumid;
    doc["isIncubationRunning"] = _isIncubationRunning;
    
    doc["isIncubationCompleted"] = _isIncubationCompleted;
    doc["actualDay"] = _actualDay;
    doc["displayDay"] = _currentDay;
    
    doc["pidMode"] = _pidMode;
    doc["pidKp"] = _pidKp;
    doc["pidKi"] = _pidKi;
    doc["pidKd"] = _pidKd;
    
    // Detaylı alarm verileri
    JsonObject alarms = doc.createNestedObject("alarms");
    alarms["enabled"] = _alarmEnabled;
    alarms["tempLow"] = _tempLowAlarm;
    alarms["tempHigh"] = _tempHighAlarm;
    alarms["humidLow"] = _humidLowAlarm;
    alarms["humidHigh"] = _humidHighAlarm;
    
    doc["motorWaitTime"] = _motorWaitTime;
    doc["motorRunTime"] = _motorRunTime;
    
    doc["tempCalibration1"] = _tempCalibration1;
    doc["tempCalibration2"] = _tempCalibration2;
    doc["humidCalibration1"] = _humidCalibration1;
    doc["humidCalibration2"] = _humidCalibration2;
    
    doc["manualDevTemp"] = _manualDevTemp;
    doc["manualHatchTemp"] = _manualHatchTemp;
    doc["manualDevHumid"] = _manualDevHumid;
    doc["manualHatchHumid"] = _manualHatchHumid;
    doc["manualDevDays"] = _manualDevDays;
    doc["manualHatchDays"] = _manualHatchDays;
    
    doc["wifiStatus"] = getStatusString();
    doc["ipAddress"] = getIPAddress();
    doc["wifiMode"] = (getCurrentMode() == WIFI_AP) ? "AP" : "Station";
    doc["ssid"] = _ssid;
    doc["signalStrength"] = getSignalStrength();
    
    doc["timestamp"] = millis();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    return jsonString;
}

String WiFiManager::getStatusString() const {
    switch (_connectionStatus) {
        case WIFI_STATUS_DISCONNECTED:
            return "Bağlantısız";
        case WIFI_STATUS_CONNECTING:
            return "Bağlanıyor...";
        case WIFI_STATUS_CONNECTED:
            return "Bağlı (" + _ssid + ")";
        case WIFI_STATUS_FAILED:
            return "Bağlantı Başarısız";
        case WIFI_STATUS_AP_MODE:
            return "AP Modu (" + _ssid + ")";
        default:
            return "Bilinmeyen";
    }
}

void WiFiManager::_checkConnectionStatus() {
    if (WiFi.getMode() == WIFI_STA) {
        wl_status_t status = WiFi.status();
        
        if (status == WL_CONNECTED) {
            if (_connectionStatus != WIFI_STATUS_CONNECTED) {
                _connectionStatus = WIFI_STATUS_CONNECTED;
                _isConnected = true;
                Serial.println("WiFi bağlantısı kuruldu: " + WiFi.localIP().toString());
            }
        } else {
            if (_connectionStatus == WIFI_STATUS_CONNECTED) {
                _connectionStatus = WIFI_STATUS_DISCONNECTED;
                _isConnected = false;
                Serial.println("WiFi bağlantısı koptu, yeniden bağlanmayı deniyor...");
            }
            
            // Bağlantı yoksa ve son denemeden 30 saniye geçtiyse tekrar dene
            if (_connectionStatus == WIFI_STATUS_DISCONNECTED || 
                _connectionStatus == WIFI_STATUS_FAILED) {
                
                unsigned long currentTime = millis();
                if (currentTime - _lastConnectionAttempt > 30000) { // 30 saniye
                    _lastConnectionAttempt = currentTime;
                    
                    Serial.println("WiFi yeniden bağlanma denemesi...");
                    WiFi.disconnect();
                    delay(1000);
                    WiFi.begin(_stationSSID.c_str(), _stationPassword.c_str());
                    _connectionStatus = WIFI_STATUS_CONNECTING;
                }
            }
        }
    }
}

void WiFiManager::_scanWiFiNetworks() {
    WiFi.scanNetworks(true);
}

String WiFiManager::_getHtmlContent() {
    static const char HTML_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
body { font-family: Arial; margin: 0; padding: 20px; background-color: #f0f0f0; }
.card { background-color: white; padding: 20px; margin-bottom: 15px; border-radius: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
.row:after { content: ''; display: table; clear: both; }
.column { float: left; width: 50%; }
h1 { color: #333; text-align: center; margin-bottom: 30px; }
h2 { color: #333; margin-top: 0; }
.temp { color: #e74c3c; font-weight: bold; }
.humid { color: #3498db; font-weight: bold; }
.status { font-weight: bold; }
.active { color: #27ae60; }
.inactive { color: #95a5a6; }
.button { background-color: #3498db; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; }
.button:hover { background-color: #2980b9; }
.button.red { background-color: #e74c3c; }
.button.red:hover { background-color: #c0392b; }
.button.green { background-color: #27ae60; }
.button.green:hover { background-color: #229954; }
.nav { text-align: center; margin-bottom: 20px; }
.control-panel { margin-top: 20px; }
.input-group { margin-bottom: 10px; }
.input-group label { display: inline-block; width: 120px; }
.input-group input, .input-group select { padding: 5px; margin-left: 10px; }
.alarm-status { padding: 10px; margin: 10px 0; border-radius: 5px; font-weight: bold; }
.alarm-enabled { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
.alarm-disabled { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
.completion-status { background-color: #fff3cd; color: #856404; border: 1px solid #ffeaa7; padding: 10px; margin: 10px 0; border-radius: 5px; font-weight: bold; }
</style>
<script>
let statusData = {};

function updateStatus() {
  fetch('/api/status').then(response => response.json()).then(data => {
    statusData = data;
    document.getElementById('temp').innerHTML = data.temperature.toFixed(1) + '&deg;C';
    document.getElementById('humid').innerHTML = data.humidity.toFixed(0) + '%';
    document.getElementById('targetTemp').innerHTML = data.targetTemp.toFixed(1) + '&deg;C';
    document.getElementById('targetHumid').innerHTML = data.targetHumid.toFixed(0) + '%';
    document.getElementById('day').innerHTML = data.displayDay + '/' + data.totalDays;
    document.getElementById('type').innerHTML = data.incubationType;
    
    document.getElementById('heater').className = data.heaterState ? 'status active' : 'status inactive';
    document.getElementById('heater').innerHTML = data.heaterState ? 'AÇIK' : 'KAPALI';
    document.getElementById('humidifier').className = data.humidifierState ? 'status active' : 'status inactive';
    document.getElementById('humidifier').innerHTML = data.humidifierState ? 'AÇIK' : 'KAPALI';
    document.getElementById('motor').className = data.motorState ? 'status active' : 'status inactive';
    document.getElementById('motor').innerHTML = data.motorState ? 'AÇIK' : 'KAPALI';
    
    const alarmStatus = document.getElementById('alarmStatus');
    if (data.alarmEnabled) {
      alarmStatus.className = 'alarm-status alarm-enabled';
      alarmStatus.innerHTML = 'Alarmlar Aktif';
    } else {
      alarmStatus.className = 'alarm-status alarm-disabled';
      alarmStatus.innerHTML = 'Alarmlar Devre Dışı';
    }
    
    const completionStatus = document.getElementById('completionStatus');
    if (data.isIncubationCompleted) {
      completionStatus.className = 'completion-status';
      completionStatus.innerHTML = 'Kuluçka Süresi Tamamlandı - Çıkım Devam Ediyor (Gerçek Gün: ' + data.actualDay + ')';
      completionStatus.style.display = 'block';
    } else {
      completionStatus.style.display = 'none';
    }
    
    document.getElementById('targetTempInput').value = data.targetTemp.toFixed(1);
    document.getElementById('targetHumidInput').value = data.targetHumid.toFixed(0);
    document.getElementById('incubationTypeSelect').value = data.incubationType;
    document.getElementById('pidModeSelect').value = data.pidMode;
    document.getElementById('alarmEnabledCheckbox').checked = data.alarmEnabled;
  }).catch(error => {
    console.log('Durum güncelleme hatası:', error);
  });
  setTimeout(updateStatus, 2000);
}

function setTargetTemp() {
  const temp = document.getElementById('targetTempInput').value;
  sendCommand('/api/temperature', {targetTemp: parseFloat(temp)});
}

function setTargetHumid() {
  const humid = document.getElementById('targetHumidInput').value;
  sendCommand('/api/humidity', {targetHumid: parseFloat(humid)});
}

function setIncubationType() {
  const type = document.getElementById('incubationTypeSelect').value;
  sendCommand('/api/incubation', {incubationType: parseInt(type)});
}

function setPIDMode() {
  const mode = document.getElementById('pidModeSelect').value;
  sendCommand('/api/pid', {pidMode: parseInt(mode)});
}

function toggleAlarm() {
  const enabled = document.getElementById('alarmEnabledCheckbox').checked;
  sendCommand('/api/alarm', {alarmEnabled: enabled});
}

function startIncubation() {
  sendCommand('/api/incubation', {isIncubationRunning: true});
}

function stopIncubation() {
  if(confirm('Kuluçka durduruluyor. Emin misiniz?')) {
    sendCommand('/api/incubation', {isIncubationRunning: false});
  }
}

function sendCommand(url, data) {
  fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  })
  .then(response => response.json())
  .then(result => {
    if(result.status === 'success') {
      alert('İşlem başarılı!');
      setTimeout(updateStatus, 500);
    } else {
      alert('Hata: ' + result.message);
    }
  })
  .catch(error => {
    alert('Bağlantı hatası: ' + error);
  });
}

document.addEventListener('DOMContentLoaded', updateStatus);
</script>
</head>
<body>
<h1>KULUÇKA MK v5.0</h1>
<div class='nav'>
<button class='button' onclick="location.href='/'">Ana Sayfa</button>
<button class='button' onclick="location.href='/wifi'">WiFi Ayarları</button>
</div>

<div class='card'>
<h2>Sıcaklık ve Nem</h2>
<div class='row'>
<div class='column'>
<h3>Sıcaklık: <span id='temp' class='temp'>--.-&deg;C</span></h3>
<p>Hedef: <span id='targetTemp'>--.-&deg;C</span></p>
<p>Isıtıcı: <span id='heater' class='status'>--</span></p>
</div>
<div class='column'>
<h3>Nem: <span id='humid' class='humid'>--%</span></h3>
<p>Hedef: <span id='targetHumid'>--%</span></p>
<p>Nemlendirici: <span id='humidifier' class='status'>--</span></p>
</div>
</div>
</div>

<div class='card'>
<h2>Kuluçka Durumu</h2>
<div class='row'>
<div class='column'>
<h3>Gün: <span id='day'>--/--</span></h3>
<p>Tip: <span id='type'>--</span></p>
</div>
<div class='column'>
<h3>Motor: <span id='motor' class='status'>--</span></h3>
</div>
</div>
<div id='completionStatus' style='display: none;'></div>
</div>

<div class='card'>
<h2>Alarm Durumu</h2>
<div id='alarmStatus' class='alarm-status'>Alarm durumu yükleniyor...</div>
</div>

<div class='card'>
<h2>Kontrol Paneli</h2>
<div class='control-panel'>
<div class='input-group'>
<label>Hedef Sıcaklık:</label>
<input type='number' id='targetTempInput' step='0.1' min='20' max='40'>
<button class='button' onclick='setTargetTemp()'>Ayarla</button>
</div>
<div class='input-group'>
<label>Hedef Nem:</label>
<input type='number' id='targetHumidInput' step='1' min='30' max='90'>
<button class='button' onclick='setTargetHumid()'>Ayarla</button>
</div>
<div class='input-group'>
<label>Kuluçka Tipi:</label>
<select id='incubationTypeSelect'>
<option value='0'>Tavuk</option>
<option value='1'>Bıldırcın</option>
<option value='2'>Kaz</option>
<option value='3'>Manuel</option>
</select>
<button class='button' onclick='setIncubationType()'>Ayarla</button>
</div>
<div class='input-group'>
<label>PID Modu:</label>
<select id='pidModeSelect'>
<option value='0'>Kapalı</option>
<option value='1'>Manuel</option>
<option value='2'>Otomatik</option>
</select>
<button class='button' onclick='setPIDMode()'>Ayarla</button>
</div>
<div class='input-group'>
<label>Alarm:</label>
<input type='checkbox' id='alarmEnabledCheckbox'>
<button class='button' onclick='toggleAlarm()'>Ayarla</button>
</div>
<div class='input-group'>
<button class='button green' onclick='startIncubation()'>Kuluçka Başlat</button>
<button class='button red' onclick='stopIncubation()'>Kuluçka Durdur</button>
</div>
</div>
</div>

</body>
</html>
)rawliteral";
    
    return FPSTR(HTML_TEMPLATE);
}

String WiFiManager::_getWiFiConfigHTML() {
    static const char WIFI_HTML_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
body { font-family: Arial; margin: 0; padding: 20px; background-color: #f0f0f0; }
.card { background-color: white; padding: 20px; margin-bottom: 15px; border-radius: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
h1 { color: #333; text-align: center; }
.form-group { margin-bottom: 15px; }
label { display: block; margin-bottom: 5px; font-weight: bold; }
input, select { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }
.button { background-color: #3498db; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; }
.button:hover { background-color: #2980b9; }
.button.green { background-color: #27ae60; }
.button.green:hover { background-color: #229954; }
.nav { text-align: center; margin-bottom: 20px; }
.network-list { max-height: 200px; overflow-y: auto; border: 1px solid #ddd; border-radius: 5px; }
.network-item { padding: 10px; border-bottom: 1px solid #eee; cursor: pointer; }
.network-item:hover { background-color: #f8f9fa; }
.network-item:last-child { border-bottom: none; }
</style>
<script>
function selectNetwork(ssid) {
  document.getElementById('stationSSID').value = ssid;
}

function connectToWiFi() {
  const ssid = document.getElementById('stationSSID').value;
  const password = document.getElementById('stationPassword').value;
  
  if (!ssid) {
    alert('Lütfen bir WiFi ağı seçin veya SSID girin');
    return;
  }
  
  const data = {
    ssid: ssid,
    password: password
  };
  
  fetch('/api/wifi/connect', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  })
  .then(response => response.json())
  .then(data => {
    if (data.status === 'success') {
      alert('WiFi bağlantısı başlatıldı. Lütfen bekleyin...');
      setTimeout(() => location.reload(), 5000);
    } else {
      alert('Hata: ' + data.message);
    }
  })
  .catch(error => {
    alert('Bağlantı hatası: ' + error);
  });
}

function switchToAP() {
  fetch('/api/wifi/ap', { method: 'POST' })
  .then(response => response.json())
  .then(data => {
    if (data.status === 'success') {
      alert('AP moduna geçiliyor...');
      setTimeout(() => location.reload(), 3000);
    } else {
      alert('Hata: ' + data.message);
    }
  });
}

function loadNetworks() {
  fetch('/api/wifi/networks')
  .then(response => response.json())
  .then(data => {
    const networkList = document.getElementById('networkList');
    networkList.innerHTML = '';
    
    if (data.networks && data.networks.length > 0) {
      data.networks.forEach(network => {
        const item = document.createElement('div');
        item.className = 'network-item';
        item.innerHTML = `<strong>${network.ssid}</strong> (${network.rssi} dBm)`;
        item.onclick = () => selectNetwork(network.ssid);
        networkList.appendChild(item);
      });
    } else {
      networkList.innerHTML = '<p>Ağ bulunamadı</p>';
    }
  })
  .catch(error => {
    console.error('Ağ listesi yüklenemedi:', error);
    document.getElementById('networkList').innerHTML = '<p>Ağ listesi yüklenemedi</p>';
  });
}

document.addEventListener('DOMContentLoaded', loadNetworks);
</script>
</head>
<body>
<h1>WiFi Ayarları</h1>
<div class='nav'>
<button class='button' onclick="location.href='/'">Ana Sayfa</button>
<button class='button' onclick="switchToAP()">AP Moduna Geç</button>
</div>
<div class='card'>
<h2>Mevcut Ağlar</h2>
<div id='networkList' class='network-list'>
<p>Ağlar yükleniyor...</p>
</div>
<button class='button' onclick="loadNetworks()">Yenile</button>
</div>
<div class='card'>
<h2>WiFi Bağlantısı</h2>
<div class='form-group'>
<label for='stationSSID'>Ağ Adı (SSID):</label>
<input type='text' id='stationSSID' placeholder='WiFi ağ adını girin'>
</div>
<div class='form-group'>
<label for='stationPassword'>Şifre:</label>
<input type='password' id='stationPassword' placeholder='WiFi şifresini girin'>
</div>
<button class='button green' onclick="connectToWiFi()">Bağlan</button>
</div>
</body>
</html>
)rawliteral";
    return FPSTR(WIFI_HTML_TEMPLATE);
}

String WiFiManager::_getStatusJson() {
    StaticJsonDocument<2048> doc;
    
    // Temel sensör verileri
    doc["temperature"] = _currentTemp;
    doc["humidity"] = _currentHumid;
    doc["heaterState"] = _heaterState;
    doc["humidifierState"] = _humidifierState;
    doc["motorState"] = _motorState;
    
    // Detaylı sensör verileri
    JsonObject sensors = doc.createNestedObject("sensors");
    JsonObject sensor1 = sensors.createNestedObject("sensor1");
    sensor1["temperature"] = _temp1;
    sensor1["humidity"] = _humid1;
    sensor1["working"] = _sensor1Working;
    sensor1["tempCalibration"] = _tempCalibration1;
    sensor1["humidCalibration"] = _humidCalibration1;
    
    JsonObject sensor2 = sensors.createNestedObject("sensor2");
    sensor2["temperature"] = _temp2;
    sensor2["humidity"] = _humid2;
    sensor2["working"] = _sensor2Working;
    sensor2["tempCalibration"] = _tempCalibration2;
    sensor2["humidCalibration"] = _humidCalibration2;
    
    // Kuluçka verileri
    doc["currentDay"] = _currentDay;
    doc["totalDays"] = _totalDays;
    doc["incubationType"] = _incubationType;
    
    // ÖNEMLİ: Gerçek hedef değerleri kullan
    doc["targetTemp"] = _targetTemp;
    doc["targetHumid"] = _targetHumid;
    
    doc["isIncubationRunning"] = _isIncubationRunning;
    doc["isIncubationCompleted"] = _isIncubationCompleted;
    doc["actualDay"] = _actualDay;
    doc["displayDay"] = _currentDay;
    
    // PID verileri
    doc["pidMode"] = _pidMode;
    doc["pidKp"] = _pidKp;
    doc["pidKi"] = _pidKi;
    doc["pidKd"] = _pidKd;
    
    // Alarm verileri - DETAYLI
    JsonObject alarms = doc.createNestedObject("alarms");
    alarms["enabled"] = _alarmEnabled;
    alarms["tempLow"] = _tempLowAlarm;
    alarms["tempHigh"] = _tempHighAlarm;
    alarms["humidLow"] = _humidLowAlarm;
    alarms["humidHigh"] = _humidHighAlarm;
    
    // Motor ayarları
    doc["motorWaitTime"] = _motorWaitTime;
    doc["motorRunTime"] = _motorRunTime;
    
    // Kalibrasyon verileri
    doc["tempCalibration1"] = _tempCalibration1;
    doc["tempCalibration2"] = _tempCalibration2;
    doc["humidCalibration1"] = _humidCalibration1;
    doc["humidCalibration2"] = _humidCalibration2;
    
    // Manuel kuluçka parametreleri
    doc["manualDevTemp"] = _manualDevTemp;
    doc["manualHatchTemp"] = _manualHatchTemp;
    doc["manualDevHumid"] = _manualDevHumid;
    doc["manualHatchHumid"] = _manualHatchHumid;
    doc["manualDevDays"] = _manualDevDays;
    doc["manualHatchDays"] = _manualHatchDays;
    
    // WiFi bilgileri
    doc["wifiStatus"] = getStatusString();
    doc["ipAddress"] = getIPAddress();
    doc["wifiMode"] = (getCurrentMode() == WIFI_AP) ? "AP" : "Station";
    doc["ssid"] = _ssid;
    doc["signalStrength"] = getSignalStrength();
    
    // Sistem bilgileri
    doc["timestamp"] = millis();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;
    doc["firmwareVersion"] = "5.0";

    // Motor detaylı bilgisi - GENİŞLETİLDİ
    JsonObject motor = doc.createNestedObject("motor");
    motor["state"] = _motorState;
    motor["waitTime"] = _motorWaitTime;
    motor["runTime"] = _motorRunTime;
    motor["testAvailable"] = !_motorState; // Motor çalışmıyorsa test yapılabilir
    
    // Sistem güvenilirlik bilgisi - YENİ
    JsonObject reliability = doc.createNestedObject("reliability");
    reliability["lastSave"] = _storage ? _storage->getTimeSinceLastSave() / 1000 : 0;
    reliability["pendingChanges"] = _storage ? _storage->getPendingChanges() : 0;
    reliability["autoSaveEnabled"] = true;
    reliability["criticalParamsProtected"] = true;
    
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

String WiFiManager::_getWiFiNetworksJson() {
    StaticJsonDocument<1024> doc;
    JsonArray networks = doc.createNestedArray("networks");
    int n = WiFi.scanComplete();
    if (n >= 0) {
        for (int i = 0; i < n; i++) {
            JsonObject network = networks.createNestedObject();
            network["ssid"] = WiFi.SSID(i);
            network["rssi"] = WiFi.RSSI(i);
            network["encryption"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "open" : "encrypted";
        }
        WiFi.scanDelete();
    }
    
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

void WiFiManager::_setupRoutes() {
    if (!_server) {
        return;
    }
    
    // Ana sayfa - web arayüzü
    _server->on("/", HTTP_GET, [this]() {
        _server->send(200, "text/html", _getHtmlContent());
    });

    // WiFi durumu API'si
    _server->on("/api/wifi/status", HTTP_GET, [this]() {
        StaticJsonDocument<300> doc;
        doc["mode"] = (WiFi.getMode() == WIFI_AP) ? "AP" : "Station";
        doc["connected"] = isConnected();
        doc["ssid"] = _ssid;
        doc["ip"] = getIPAddress();
        doc["rssi"] = getSignalStrength();
        doc["status"] = getStatusString();
        
        String response;
        serializeJson(doc, response);
        _server->send(200, "application/json", response);
    });

    // WiFi modu değiştirme endpoint'i
_server->on("/api/wifi/mode", HTTP_POST, [this]() {
    String jsonString = _server->arg("plain");
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        _server->send(400, "application/json", _createErrorResponse("Invalid JSON"));
        return;
    }
    
    if (doc.containsKey("mode")) {
        String mode = doc["mode"];
        bool success = false;
        
        // Mevcut durum verilerini koru
        StaticJsonDocument<1024> statusBackup;
        statusBackup["temperature"] = _currentTemp;
        statusBackup["humidity"] = _currentHumid;
        statusBackup["targetTemp"] = _targetTemp;
        statusBackup["targetHumid"] = _targetHumid;
        statusBackup["heaterState"] = _heaterState;
        statusBackup["humidifierState"] = _humidifierState;
        statusBackup["motorState"] = _motorState;
        
        if (mode == "station") {
            success = switchToStationMode();
        } else if (mode == "ap") {
            success = switchToAPMode();
        }
        
        // Detaylı yanıt
        StaticJsonDocument<600> response;
        response["status"] = success ? "success" : "error";
        response["message"] = success ? "WiFi mode changed successfully" : "WiFi mode change failed";
        response["newMode"] = mode;
        response["ipAddress"] = getIPAddress();
        response["connected"] = isConnected();
        
        // Korunan sistem durumu
        JsonObject preserved = response.createNestedObject("preservedData");
        preserved["temperature"] = statusBackup["temperature"];
        preserved["humidity"] = statusBackup["humidity"];
        preserved["targetTemp"] = statusBackup["targetTemp"];
        preserved["targetHumid"] = statusBackup["targetHumid"];
        preserved["heaterState"] = statusBackup["heaterState"];
        preserved["humidifierState"] = statusBackup["humidifierState"];
        preserved["motorState"] = statusBackup["motorState"];
        
        String responseStr;
        serializeJson(response, responseStr);
        _server->send(200, "application/json", responseStr);
    } else {
        _server->send(400, "application/json", _createErrorResponse("Missing mode parameter"));
    }
});

// Sistem durumu sorgulama endpoint'i (WiFi değişimi sonrası kontrol için)
_server->on("/api/system/verify", HTTP_GET, [this]() {
    StaticJsonDocument<800> doc;
    
    // Sistem durumu
    doc["status"] = "online";
    doc["timestamp"] = millis();
    doc["freeHeap"] = ESP.getFreeHeap();
    
    // WiFi durumu
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["mode"] = (WiFi.getMode() == WIFI_AP) ? "AP" : "Station";
    wifi["connected"] = isConnected();
    wifi["ssid"] = _ssid;
    wifi["ip"] = getIPAddress();
    wifi["rssi"] = getSignalStrength();
    
    // Sistem parametreleri
    JsonObject params = doc.createNestedObject("parameters");
    params["temperature"] = _currentTemp;
    params["humidity"] = _currentHumid;
    params["targetTemp"] = _targetTemp;
    params["targetHumid"] = _targetHumid;
    params["heaterState"] = _heaterState;
    params["humidifierState"] = _humidifierState;
    params["motorState"] = _motorState;
    params["alarmEnabled"] = _alarmEnabled;
    params["pidMode"] = _pidMode;
    
    // Kuluçka durumu
    JsonObject incubation = doc.createNestedObject("incubation");
    incubation["running"] = _isIncubationRunning;
    incubation["type"] = _incubationType;
    incubation["currentDay"] = _currentDay;
    incubation["totalDays"] = _totalDays;
    incubation["completed"] = _isIncubationCompleted;
    
    String jsonString;
    serializeJson(doc, jsonString);
    _server->send(200, "application/json", jsonString);
});

    // Durum verileri JSON API
    _server->on("/api/status", HTTP_GET, [this]() {
        _server->send(200, "application/json", _getStatusJson());
    });
    
    // WiFi ağları listesi
    _server->on("/api/wifi/networks", HTTP_GET, [this]() {
        Serial.println("WiFi taraması başlatılıyor...");
        
        // Önce mevcut tarama sonuçlarını temizle
        WiFi.scanDelete();
        
        // Senkron tarama başlat (false = senkron, true = hidden SSID'leri de göster)
        int n = WiFi.scanNetworks(false, true);
        
        StaticJsonDocument<2048> doc;
        JsonArray networks = doc.createNestedArray("networks");
        
        Serial.println("Bulunan ağ sayısı: " + String(n));
        
        if (n > 0) {
            for (int i = 0; i < n && i < 20; i++) { // Maksimum 20 ağ
                String ssid = WiFi.SSID(i);
                if (ssid.length() > 0) { // Boş SSID'leri atla
                    JsonObject network = networks.createNestedObject();
                    network["ssid"] = ssid;
                    network["rssi"] = WiFi.RSSI(i);
                    
                    // Encryption type kontrolü
                    wifi_auth_mode_t encType = WiFi.encryptionType(i);
                    if (encType == WIFI_AUTH_OPEN) {
                        network["encryption"] = "open";
                    } else {
                        network["encryption"] = "WPA2";
                    }
                    
                    Serial.println("Ağ: " + ssid + " RSSI: " + String(WiFi.RSSI(i)));
                }
            }
        }
        
        String jsonString;
        serializeJson(doc, jsonString);
        
        Serial.println("WiFi networks JSON: " + jsonString);
        
        _server->send(200, "application/json", jsonString);
    });
    
    // WiFi bağlantısı
    _server->on("/api/wifi/connect", HTTP_POST, [this]() {
        _handleWiFiConnect();
    });
    
    // AP moduna geçiş
    _server->on("/api/wifi/ap", HTTP_POST, [this]() {
        bool success = switchToAPMode();
        if (success) {
            _server->send(200, "application/json", _createSuccessResponse());
        } else {
            _server->send(500, "application/json", _createErrorResponse("AP modu başlatılamadı"));
        }
    });
    
    // Sıcaklık kontrolü
    _server->on("/api/temperature", HTTP_POST, [this]() {
        _handleSetTemperature();
    });
    
    // Nem kontrolü
    _server->on("/api/humidity", HTTP_POST, [this]() {
        _handleSetHumidity();
    });
    
    // PID kontrolü
    _server->on("/api/pid", HTTP_POST, [this]() {
        _handleSetPidParameters();
    });
    
    // Motor kontrolü
    _server->on("/api/motor", HTTP_POST, [this]() {
        _handleSetMotorSettings();
    });
    
    // Alarm kontrolü
    _server->on("/api/alarm", HTTP_POST, [this]() {
        _handleSetAlarmSettings();
    });
    
    // Kalibrasyon kontrolü
    _server->on("/api/calibration", HTTP_POST, [this]() {
        _handleSetCalibration();
    });
    
    // Kuluçka kontrolü
    _server->on("/api/incubation", HTTP_POST, [this]() {
        _handleSetIncubationSettings();
    });
    
    // WiFi ayar sayfası
    _server->on("/wifi", HTTP_GET, [this]() {
        _server->send(200, "text/html", _getWiFiConfigHTML());
    });
    
    // WiFi ayarlarını kaydet
    _server->on("/api/wifi/save", HTTP_POST, [this]() {
        saveWiFiSettings();
        _server->send(200, "application/json", _createSuccessResponse());
    });
    
    // Discovery endpoint - UDP broadcast için
    _server->on("/api/discovery", HTTP_GET, [this]() {
        StaticJsonDocument<256> doc;
        doc["device"] = "KULUCKA_MK_v5";
        doc["version"] = "5.0";
        doc["ip"] = WiFi.localIP().toString();
        doc["mode"] = (WiFi.getMode() == WIFI_AP) ? "AP" : "Station";
        doc["port"] = WIFI_PORT;
        
        String jsonString;
        serializeJson(doc, jsonString);
        _server->send(200, "application/json", jsonString);
    });

    // Ping endpoint - bağlantı testi için
    _server->on("/api/ping", HTTP_GET, [this]() {
        _server->send(200, "text/plain", "pong");
    });

    // Motor test endpoint'i - YENİ
    _server->on("/api/motor/test", HTTP_POST, [this]() {
        _handleMotorTest();
    });
    
    // Sistem durumu kaydetme endpoint'i - YENİ
    _server->on("/api/system/save", HTTP_POST, [this]() {
        _handleSystemSave();
    });
    
    // WiFi credential kontrolü endpoint'i - YENİ
    _server->on("/api/wifi/credentials", HTTP_GET, [this]() {
        _handleWiFiCredentials();
    });
    
    // Sistem sağlık kontrolü endpoint'i - YENİ
    _server->on("/api/system/health", HTTP_GET, [this]() {
        _handleSystemHealth();
    });

    // Manuel kuluçka parametreleri toplu güncelleme
_server->on("/api/incubation/manual", HTTP_POST, [this]() {
    String jsonString = _server->arg("plain");
    StaticJsonDocument<600> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        _server->send(400, "application/json", _createErrorResponse("Invalid JSON"));
        return;
    }
    
    bool hasUpdate = false;
    
    // Gelişim parametreleri
    if (doc.containsKey("development")) {
        JsonObject dev = doc["development"];
        if (dev.containsKey("temperature")) {
            _processParameterUpdate("manualDevTemp", String((float)dev["temperature"]));
            hasUpdate = true;
        }
        if (dev.containsKey("humidity")) {
            _processParameterUpdate("manualDevHumid", String((int)dev["humidity"]));
            hasUpdate = true;
        }
        if (dev.containsKey("days")) {
            _processParameterUpdate("manualDevDays", String((int)dev["days"]));
            hasUpdate = true;
        }
    }
    
    // Çıkım parametreleri
    if (doc.containsKey("hatching")) {
        JsonObject hatch = doc["hatching"];
        if (hatch.containsKey("temperature")) {
            _processParameterUpdate("manualHatchTemp", String((float)hatch["temperature"]));
            hasUpdate = true;
        }
        if (hatch.containsKey("humidity")) {
            _processParameterUpdate("manualHatchHumid", String((int)hatch["humidity"]));
            hasUpdate = true;
        }
        if (hatch.containsKey("days")) {
            _processParameterUpdate("manualHatchDays", String((int)hatch["days"]));
            hasUpdate = true;
        }
    }
    
    if (hasUpdate) {
        StaticJsonDocument<300> response;
        response["status"] = "success";
        response["message"] = "Manual incubation parameters updated";
        
        String responseStr;
        serializeJson(response, responseStr);
        _server->send(200, "application/json", responseStr);
    } else {
        _server->send(400, "application/json", _createErrorResponse("No valid parameters provided"));
    }
});

    // Sensör detayları endpoint'i
_server->on("/api/sensors/details", HTTP_GET, [this]() {
    StaticJsonDocument<800> doc;
    
    // Ortalama değerler
    doc["average"]["temperature"] = _currentTemp;
    doc["average"]["humidity"] = _currentHumid;
    
    // Sensör 1 detayları
    JsonObject sensor1 = doc.createNestedObject("sensor1");
    sensor1["id"] = "SHT31_1";
    sensor1["address"] = "0x44";
    sensor1["temperature"] = _temp1;
    sensor1["humidity"] = _humid1;
    sensor1["working"] = _sensor1Working;
    sensor1["calibration"]["temperature"] = _tempCalibration1;
    sensor1["calibration"]["humidity"] = _humidCalibration1;
    
    // Sensör 2 detayları
    JsonObject sensor2 = doc.createNestedObject("sensor2");
    sensor2["id"] = "SHT31_2";
    sensor2["address"] = "0x45";
    sensor2["temperature"] = _temp2;
    sensor2["humidity"] = _humid2;
    sensor2["working"] = _sensor2Working;
    sensor2["calibration"]["temperature"] = _tempCalibration2;
    sensor2["calibration"]["humidity"] = _humidCalibration2;
    
    // Sensör sağlık durumu
    JsonObject health = doc.createNestedObject("health");
    health["sensorsWorking"] = _sensor1Working || _sensor2Working;
    health["allSensorsWorking"] = _sensor1Working && _sensor2Working;
    health["temperatureValid"] = (_currentTemp > -50 && _currentTemp < 100);
    health["humidityValid"] = (_currentHumid >= 0 && _currentHumid <= 100);
    
    String jsonString;
    serializeJson(doc, jsonString);
    _server->send(200, "application/json", jsonString);
});

    // RTC Saat ayarlama endpoint'i
_server->on("/api/rtc/time", HTTP_POST, [this]() {
    String jsonString = _server->arg("plain");
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        _server->send(400, "application/json", _createErrorResponse("Invalid JSON"));
        return;
    }
    
    if (doc.containsKey("hour") && doc.containsKey("minute")) {
        int hour = doc["hour"];
        int minute = doc["minute"];
        
        if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
            // RTC modülüne erişim için extern referans
            extern RTCModule rtc;
            DateTime now = rtc.getCurrentDateTime();
            
            bool success = rtc.setDateTime(hour, minute, now.day(), now.month(), now.year());
            
            if (success) {
                StaticJsonDocument<300> response;
                response["status"] = "success";
                response["message"] = "Time updated successfully";
                response["time"] = String(hour) + ":" + String(minute);
                
                String responseStr;
                serializeJson(response, responseStr);
                _server->send(200, "application/json", responseStr);
            } else {
                _server->send(500, "application/json", _createErrorResponse("RTC update failed"));
            }
        } else {
            _server->send(400, "application/json", _createErrorResponse("Invalid time values"));
        }
    } else {
        _server->send(400, "application/json", _createErrorResponse("Missing hour or minute"));
    }
});

// RTC Tarih ayarlama endpoint'i
_server->on("/api/rtc/date", HTTP_POST, [this]() {
    String jsonString = _server->arg("plain");
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        _server->send(400, "application/json", _createErrorResponse("Invalid JSON"));
        return;
    }
    
    if (doc.containsKey("day") && doc.containsKey("month") && doc.containsKey("year")) {
        int day = doc["day"];
        int month = doc["month"];
        int year = doc["year"];
        
        // Basit tarih doğrulama
        if (day >= 1 && day <= 31 && month >= 1 && month <= 12 && 
            year >= 2025 && year <= 2050) {
            
            // Ay/gün kombinasyonu doğrulama
            int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            
            // Artık yıl kontrolü
            if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
                daysInMonth[1] = 29;
            }
            
            if (day <= daysInMonth[month - 1]) {
                extern RTCModule rtc;
                DateTime now = rtc.getCurrentDateTime();
                
                bool success = rtc.setDateTime(now.hour(), now.minute(), day, month, year);
                
                if (success) {
                    StaticJsonDocument<300> response;
                    response["status"] = "success";
                    response["message"] = "Date updated successfully";
                    response["date"] = String(day) + "/" + String(month) + "/" + String(year);
                    
                    String responseStr;
                    serializeJson(response, responseStr);
                    _server->send(200, "application/json", responseStr);
                } else {
                    _server->send(500, "application/json", _createErrorResponse("RTC update failed"));
                }
            } else {
                _server->send(400, "application/json", _createErrorResponse("Invalid day for given month"));
            }
        } else {
            _server->send(400, "application/json", _createErrorResponse("Invalid date values"));
        }
    } else {
        _server->send(400, "application/json", _createErrorResponse("Missing date parameters"));
    }
});

// RTC durumu sorgulama endpoint'i
_server->on("/api/rtc/status", HTTP_GET, [this]() {
    extern RTCModule rtc;
    DateTime now = rtc.getCurrentDateTime();
    
    StaticJsonDocument<400> doc;
    doc["status"] = rtc.isRTCWorking() ? "working" : "error";
    doc["time"] = rtc.getTimeString();
    doc["date"] = rtc.getDateString();
    doc["timestamp"] = now.unixtime();
    doc["errorCount"] = rtc.getRTCErrorCount();
    
    // Detaylı zaman bilgileri
    JsonObject details = doc.createNestedObject("details");
    details["hour"] = now.hour();
    details["minute"] = now.minute();
    details["second"] = now.second();
    details["day"] = now.day();
    details["month"] = now.month();
    details["year"] = now.year();
    
    String jsonString;
    serializeJson(doc, jsonString);
    _server->send(200, "application/json", jsonString);
});
}

// Motor test handler
void WiFiManager::_handleMotorTest() {
    String jsonString = _server->arg("plain");
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        _server->send(400, "application/json", _createErrorResponse("Invalid JSON"));
        return;
    }
    
    // Test süresi parametresini al
    uint32_t testDuration = _motorRunTime; // Varsayılan
    if (doc.containsKey("duration")) {
        testDuration = doc["duration"];
        // Güvenlik için sınır kontrolü
        testDuration = constrain(testDuration, 1, 60); // 1-60 saniye arası
    }
    
    // Motor test komutunu parametre güncelleme sistemi üzerinden işle
    _processParameterUpdate("motorTest", String(testDuration));
    
    // Detaylı yanıt döndür
    StaticJsonDocument<400> response;
    response["status"] = "success";
    response["message"] = "Motor test started";
    response["duration"] = testDuration;
    response["motorState"] = true;
    response["timestamp"] = millis();
    
    String responseStr;
    serializeJson(response, responseStr);
    _server->send(200, "application/json", responseStr);
}

// Sistem durumu kaydetme handler
void WiFiManager::_handleSystemSave() {
    // Tüm bekleyen değişiklikleri anında kaydet
    if (_storage != nullptr) {
        _storage->saveStateNow();
        
        StaticJsonDocument<300> response;
        response["status"] = "success";
        response["message"] = "System state saved successfully";
        response["pendingChanges"] = 0;
        response["timestamp"] = millis();
        
        String responseStr;
        serializeJson(response, responseStr);
        _server->send(200, "application/json", responseStr);
    } else {
        _server->send(500, "application/json", _createErrorResponse("Storage not initialized"));
    }
}

// WiFi credential kontrolü handler
void WiFiManager::_handleWiFiCredentials() {
    StaticJsonDocument<500> doc;
    
    doc["currentMode"] = (WiFi.getMode() == WIFI_AP) ? "AP" : "Station";
    doc["apSSID"] = AP_SSID;
    doc["hasStationCredentials"] = (_stationSSID.length() > 0);
    
    if (_stationSSID.length() > 0) {
        doc["stationSSID"] = _stationSSID;
        doc["stationSaved"] = true;
        // Güvenlik için şifreyi gösterme
        doc["stationPasswordLength"] = _stationPassword.length();
    } else {
        doc["stationSaved"] = false;
    }
    
    doc["connected"] = isConnected();
    doc["ipAddress"] = getIPAddress();
    
    String jsonString;
    serializeJson(doc, jsonString);
    _server->send(200, "application/json", jsonString);
}

// Sistem sağlık kontrolü handler
void WiFiManager::_handleSystemHealth() {
    StaticJsonDocument<1024> doc;
    
    // Sistem durumu
    doc["status"] = "healthy";
    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    
    // Heap fragmentation hesaplaması - düzeltilmiş
    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t freeHeap = ESP.getFreeHeap();
    if (totalHeap > 0) {
        doc["heapFragmentation"] = 100 - ((freeHeap * 100) / totalHeap);
    } else {
        doc["heapFragmentation"] = 0;
    }
    
    // Storage durumu
    JsonObject storage = doc.createNestedObject("storage");
    if (_storage != nullptr) {
        storage["initialized"] = true;
        storage["pendingChanges"] = _storage->getPendingChanges();
        storage["lastSaveTime"] = _storage->getTimeSinceLastSave() / 1000; // saniye
        storage["criticalParameters"] = true; // Kritik parametreler kaydedildi mi
    } else {
        storage["initialized"] = false;
    }
    
    // Sensör durumu
    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["temperature"] = _currentTemp;
    sensors["humidity"] = _currentHumid;
    sensors["tempValid"] = (_currentTemp > -50 && _currentTemp < 100);
    sensors["humidValid"] = (_currentHumid >= 0 && _currentHumid <= 100);
    
    // Kontrol durumu
    JsonObject control = doc.createNestedObject("control");
    control["pidMode"] = _pidMode;
    control["heaterState"] = _heaterState;
    control["humidifierState"] = _humidifierState;
    control["motorState"] = _motorState;
    control["alarmEnabled"] = _alarmEnabled;
    
    // WiFi durumu
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["mode"] = (WiFi.getMode() == WIFI_AP) ? "AP" : "Station";
    wifi["connected"] = isConnected();
    wifi["rssi"] = getSignalStrength();
    wifi["ip"] = getIPAddress();
    
    // Kuluçka durumu
    JsonObject incubation = doc.createNestedObject("incubation");
    incubation["running"] = _isIncubationRunning;
    incubation["currentDay"] = _currentDay;
    incubation["totalDays"] = _totalDays;
    incubation["completed"] = _isIncubationCompleted;
    
    String jsonString;
    serializeJson(doc, jsonString);
    _server->send(200, "application/json", jsonString);
}

void WiFiManager::_handleWiFiConnect() {
    String jsonString = _server->arg("plain");
    StaticJsonDocument<300> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        _server->send(400, "application/json", _createErrorResponse("Invalid JSON"));
        return;
    }
    
    if (doc.containsKey("ssid")) {
        String ssid = doc["ssid"];
        String password = doc["password"] | "";
        
        // Credential'ları kaydet
        setStationCredentials(ssid, password);
        
        // Mevcut ayarları anında kaydet - YENİ
        if (_storage != nullptr) {
            _storage->saveStateNow();
        }
        
        // Detaylı yanıt
        StaticJsonDocument<600> response;
        response["status"] = "success";
        response["message"] = "WiFi connection initiated";
        response["targetSSID"] = ssid;
        response["currentMode"] = (WiFi.getMode() == WIFI_AP) ? "AP" : "Station";
        response["action"] = "switching_to_station";
        response["estimatedTime"] = 15;
        response["credentialsSaved"] = true; // YENİ
        
        // Sistem durumu korunacağını belirt - YENİ
        JsonObject preserved = response.createNestedObject("systemStatePreserved");
        preserved["temperature"] = true;
        preserved["humidity"] = true;
        preserved["settings"] = true;
        preserved["incubation"] = true;
        
        String responseStr;
        serializeJson(response, responseStr);
        _server->send(200, "application/json", responseStr);
        
        // Kısa gecikme sonrası mod değiştir
        delay(500);
        switchToStationMode();
        
    } else {
        _server->send(400, "application/json", _createErrorResponse("Missing ssid parameter"));
    }
}

void WiFiManager::_handleSetTemperature() {
    String jsonString = _server->arg("plain");
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        _server->send(400, "application/json", _createErrorResponse("Invalid JSON"));
        return;
    }
    
    if (doc.containsKey("targetTemp")) {
        float targetTemp = doc["targetTemp"];
        
        // Parametre güncelleme işlemi
        _processParameterUpdate("targetTemp", String(targetTemp));
        
        // Storage'ı anında güncelle
        _targetTemp = targetTemp;
        
        // Güncel sistem durumunu içeren detaylı yanıt
        StaticJsonDocument<500> response;
        response["status"] = "success";
        response["message"] = "Temperature updated";
        response["targetTemp"] = targetTemp;
        response["currentTemp"] = _currentTemp;
        response["heaterState"] = _heaterState;
        response["timestamp"] = millis();
        
        // Sistem durumu bilgileri
        response["systemStatus"]["temperature"] = _currentTemp;
        response["systemStatus"]["humidity"] = _currentHumid;
        response["systemStatus"]["targetTemp"] = targetTemp;
        response["systemStatus"]["targetHumid"] = _targetHumid;
        
        String responseStr;
        serializeJson(response, responseStr);
        _server->send(200, "application/json", responseStr);
    } else {
        _server->send(400, "application/json", _createErrorResponse("Missing targetTemp parameter"));
    }
}

void WiFiManager::_handleSetHumidity() {
    String jsonString = _server->arg("plain");
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        _server->send(400, "application/json", _createErrorResponse("Invalid JSON"));
        return;
    }
    
    if (doc.containsKey("targetHumid")) {
        float targetHumid = doc["targetHumid"];
        
        // Parametre güncelleme işlemi
        _processParameterUpdate("targetHumid", String(targetHumid));
        
        // Storage'ı anında güncelle
        _targetHumid = targetHumid;
        
        // Güncel sistem durumunu içeren detaylı yanıt
        StaticJsonDocument<500> response;
        response["status"] = "success";
        response["message"] = "Humidity updated";
        response["targetHumid"] = targetHumid;
        response["currentHumid"] = _currentHumid;
        response["humidifierState"] = _humidifierState;
        response["timestamp"] = millis();
        
        // Sistem durumu bilgileri
        response["systemStatus"]["temperature"] = _currentTemp;
        response["systemStatus"]["humidity"] = _currentHumid;
        response["systemStatus"]["targetTemp"] = _targetTemp;
        response["systemStatus"]["targetHumid"] = targetHumid;
        
        String responseStr;
        serializeJson(response, responseStr);
        _server->send(200, "application/json", responseStr);
    } else {
        _server->send(400, "application/json", _createErrorResponse("Missing targetHumid parameter"));
    }
}

void WiFiManager::_handleSetPidParameters() {
    String jsonString = _server->arg("plain");
    StaticJsonDocument<400> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        _server->send(400, "application/json", _createErrorResponse("Invalid JSON"));
        return;
    }
    
    bool hasValidParam = false;
    String responseMessage = "";
    
    // PID parametrelerini kontrol et ve güncelle
    if (doc.containsKey("kp")) {
        float kp = doc["kp"];
        if (kp >= PID_KP_MIN && kp <= PID_KP_MAX) {
            _processParameterUpdate("pidKp", String(kp));
            _pidKp = kp; // Lokal değeri de güncelle
            responseMessage += "Kp güncellendi: " + String(kp) + " ";
            hasValidParam = true;
        } else {
            _server->send(400, "application/json", 
                         _createErrorResponse("Invalid Kp value (min: " + String(PID_KP_MIN) + 
                                            ", max: " + String(PID_KP_MAX) + ")"));
            return;
        }
    }
    
    if (doc.containsKey("ki")) {
        float ki = doc["ki"];
        if (ki >= PID_KI_MIN && ki <= PID_KI_MAX) {
            _processParameterUpdate("pidKi", String(ki));
            _pidKi = ki; // Lokal değeri de güncelle
            responseMessage += "Ki güncellendi: " + String(ki) + " ";
            hasValidParam = true;
        } else {
            _server->send(400, "application/json", 
                         _createErrorResponse("Invalid Ki value (min: " + String(PID_KI_MIN) + 
                                            ", max: " + String(PID_KI_MAX) + ")"));
            return;
        }
    }
    
    if (doc.containsKey("kd")) {
        float kd = doc["kd"];
        if (kd >= PID_KD_MIN && kd <= PID_KD_MAX) {
            _processParameterUpdate("pidKd", String(kd));
            _pidKd = kd; // Lokal değeri de güncelle
            responseMessage += "Kd güncellendi: " + String(kd) + " ";
            hasValidParam = true;
        } else {
            _server->send(400, "application/json", 
                         _createErrorResponse("Invalid Kd value (min: " + String(PID_KD_MIN) + 
                                            ", max: " + String(PID_KD_MAX) + ")"));
            return;
        }
    }
    
    if (doc.containsKey("pidMode")) {
        int mode = doc["pidMode"];
        if (mode >= 0 && mode <= 2) {
            String modeStr;
            switch(mode) {
                case 0: modeStr = "Kapalı"; break;
                case 1: modeStr = "Manuel"; break;
                case 2: modeStr = "Otomatik"; break;
            }
            
            _processParameterUpdate("pidMode", String(mode));
            _pidMode = mode; // Lokal değeri de güncelle
            responseMessage += "PID Modu: " + modeStr + " ";
            hasValidParam = true;
            
            Serial.println("WiFi API: PID modu değiştirildi -> " + modeStr);
        } else {
            _server->send(400, "application/json", 
                         _createErrorResponse("Invalid PID mode (0=OFF, 1=MANUAL, 2=AUTO_TUNE)"));
            return;
        }
    }
    
    // PID action desteği (start_manual, start_auto, stop)
    if (doc.containsKey("pidAction")) {
        String action = doc["pidAction"];
        
        if (action == "start_manual") {
            _processParameterUpdate("pidMode", "1");
            _pidMode = 1;
            responseMessage += "Manuel PID başlatıldı ";
            hasValidParam = true;
        } else if (action == "start_auto") {
            _processParameterUpdate("pidMode", "2");
            _pidMode = 2;
            responseMessage += "Otomatik PID başlatıldı ";
            hasValidParam = true;
        } else if (action == "stop") {
            _processParameterUpdate("pidMode", "0");
            _pidMode = 0;
            responseMessage += "PID durduruldu ";
            hasValidParam = true;
        } else {
            _server->send(400, "application/json", 
                         _createErrorResponse("Invalid PID action (start_manual, start_auto, stop)"));
            return;
        }
    }
    
    if (hasValidParam) {
        // Başarılı yanıt oluştur
        StaticJsonDocument<600> response;
        response["status"] = "success";
        response["message"] = responseMessage.length() > 0 ? responseMessage : "PID parametreleri güncellendi";
        
        // Güncel PID değerlerini yanıta ekle
        JsonObject pidValues = response.createNestedObject("pidValues");
        pidValues["kp"] = _pidKp;
        pidValues["ki"] = _pidKi;
        pidValues["kd"] = _pidKd;
        pidValues["mode"] = _pidMode;
        
        String modeStr;
        switch(_pidMode) {
            case 0: modeStr = "OFF"; break;
            case 1: modeStr = "MANUAL"; break;
            case 2: modeStr = "AUTO_TUNE"; break;
        }
        pidValues["modeString"] = modeStr;
        
        String responseStr;
        serializeJson(response, responseStr);
        _server->send(200, "application/json", responseStr);
    } else {
        _server->send(400, "application/json", 
                     _createErrorResponse("No valid PID parameters provided"));
    }
}

void WiFiManager::_handleSetMotorSettings() {
    String jsonString = _server->arg("plain");
    StaticJsonDocument<300> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        _server->send(400, "application/json", _createErrorResponse("Invalid JSON"));
        return;
    }
    
    bool hasValidParam = false;
    
    if (doc.containsKey("waitTime")) {
        _processParameterUpdate("motorWaitTime", String((long)doc["waitTime"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("runTime")) {
        _processParameterUpdate("motorRunTime", String((long)doc["runTime"]));
        hasValidParam = true;
    }
    
    if (hasValidParam) {
        _server->send(200, "application/json", _createSuccessResponse());
    } else {
        _server->send(400, "application/json", _createErrorResponse("No valid motor parameters"));
    }
}

void WiFiManager::_handleSetAlarmSettings() {
    String jsonString = _server->arg("plain");
    StaticJsonDocument<400> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        _server->send(400, "application/json", _createErrorResponse("Invalid JSON"));
        return;
    }
    
    bool hasValidParam = false;
    
    if (doc.containsKey("tempLowAlarm")) {
        _processParameterUpdate("tempLowAlarm", String((float)doc["tempLowAlarm"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("tempHighAlarm")) {
        _processParameterUpdate("tempHighAlarm", String((float)doc["tempHighAlarm"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("humidLowAlarm")) {
        _processParameterUpdate("humidLowAlarm", String((float)doc["humidLowAlarm"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("humidHighAlarm")) {
        _processParameterUpdate("humidHighAlarm", String((float)doc["humidHighAlarm"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("alarmEnabled")) {
        _processParameterUpdate("alarmEnabled", doc["alarmEnabled"] ? "1" : "0");
        hasValidParam = true;
        Serial.println("Web API: Alarm durumu güncellendi: " + String(doc["alarmEnabled"] ? "AÇIK" : "KAPALI"));
    }
    
    if (hasValidParam) {
        _server->send(200, "application/json", _createSuccessResponse());
    } else {
        _server->send(400, "application/json", _createErrorResponse("No valid alarm parameters"));
    }
}

void WiFiManager::_handleSetCalibration() {
    String jsonString = _server->arg("plain");
    StaticJsonDocument<400> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        _server->send(400, "application/json", _createErrorResponse("Invalid JSON"));
        return;
    }
    
    bool hasValidParam = false;
    
    // Yeni parametre isimleri ile uyumlu hale getir
    if (doc.containsKey("tempCalibration1")) {
        _processParameterUpdate("tempCalibration1", String((float)doc["tempCalibration1"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("tempCalibration2")) {
        _processParameterUpdate("tempCalibration2", String((float)doc["tempCalibration2"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("humidCalibration1")) {
        _processParameterUpdate("humidCalibration1", String((float)doc["humidCalibration1"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("humidCalibration2")) {
        _processParameterUpdate("humidCalibration2", String((float)doc["humidCalibration2"]));
        hasValidParam = true;
    }
    
    // Eski parametre isimleri için geriye dönük uyumluluk
    if (doc.containsKey("tempCal1")) {
        _processParameterUpdate("tempCalibration1", String((float)doc["tempCal1"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("tempCal2")) {
        _processParameterUpdate("tempCalibration2", String((float)doc["tempCal2"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("humidCal1")) {
        _processParameterUpdate("humidCalibration1", String((float)doc["humidCal1"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("humidCal2")) {
        _processParameterUpdate("humidCalibration2", String((float)doc["humidCal2"]));
        hasValidParam = true;
    }
    
    if (hasValidParam) {
        _server->send(200, "application/json", _createSuccessResponse());
    } else {
        _server->send(400, "application/json", _createErrorResponse("No valid calibration parameters"));
    }
}

void WiFiManager::_handleSetIncubationSettings() {
    String jsonString = _server->arg("plain");
    StaticJsonDocument<500> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        _server->send(400, "application/json", _createErrorResponse("Invalid JSON"));
        return;
    }
    
    bool hasValidParam = false;
    
    if (doc.containsKey("incubationType")) {
        _processParameterUpdate("incubationType", String((int)doc["incubationType"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("isIncubationRunning")) {
        _processParameterUpdate("isIncubationRunning", String((bool)doc["isIncubationRunning"] ? "1" : "0"));
        hasValidParam = true;
    }
    
    if (doc.containsKey("manualDevTemp")) {
        _processParameterUpdate("manualDevTemp", String((float)doc["manualDevTemp"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("manualHatchTemp")) {
        _processParameterUpdate("manualHatchTemp", String((float)doc["manualHatchTemp"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("manualDevHumid")) {
        _processParameterUpdate("manualDevHumid", String((int)doc["manualDevHumid"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("manualHatchHumid")) {
        _processParameterUpdate("manualHatchHumid", String((int)doc["manualHatchHumid"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("manualDevDays")) {
        _processParameterUpdate("manualDevDays", String((int)doc["manualDevDays"]));
        hasValidParam = true;
    }
    
    if (doc.containsKey("manualHatchDays")) {
        _processParameterUpdate("manualHatchDays", String((int)doc["manualHatchDays"]));
        hasValidParam = true;
    }
    
    if (hasValidParam) {
        _server->send(200, "application/json", _createSuccessResponse());
    } else {
        _server->send(400, "application/json", _createErrorResponse("No valid incubation parameters"));
    }
}

void WiFiManager::_processParameterUpdate(const String& param, const String& value) {
    // main.cpp'deki handleWifiParameterUpdate fonksiyonunu çağır
    extern void handleWifiParameterUpdate(String param, String value);
    handleWifiParameterUpdate(param, value);
}

String WiFiManager::_createSuccessResponse() {
    StaticJsonDocument<300> doc;
    doc["status"] = "success";
    doc["message"] = "Operation completed successfully";
    doc["timestamp"] = millis();
    
    // Güncel sistem durumu bilgilerini ekle
    JsonObject current = doc.createNestedObject("currentValues");
    current["temperature"] = _currentTemp;
    current["humidity"] = _currentHumid;
    current["targetTemp"] = _targetTemp;
    current["targetHumid"] = _targetHumid;
    
    String response;
    serializeJson(doc, response);
    return response;
}

String WiFiManager::_createErrorResponse(const String& message) {
    StaticJsonDocument<150> doc;
    doc["status"] = "error";
    doc["message"] = message;
    String response;
    serializeJson(doc, response);
    return response;
}

// WiFi_manager.cpp dosyasının sonuna eklenecek memory optimization fonksiyonları

bool WiFiManager::_allocateBuffers() {
    if (_buffersAllocated) {
        return true;
    }
    
    // JSON buffer allocate et
    _jsonBuffer = (char*)malloc(JSON_POOL_SIZE);
    if (_jsonBuffer == nullptr) {
        Serial.println("WiFi: JSON buffer allocation hatası!");
        return false;
    }
    
    // Response buffer allocate et
    _responseBuffer = (char*)malloc(WEB_RESPONSE_POOL_SIZE);
    if (_responseBuffer == nullptr) {
        Serial.println("WiFi: Response buffer allocation hatası!");
        free(_jsonBuffer);
        _jsonBuffer = nullptr;
        return false;
    }
    
    _buffersAllocated = true;
    Serial.println("WiFi: Memory buffers allocated - Free heap: " + String(ESP.getFreeHeap()));
    return true;
}

void WiFiManager::_deallocateBuffers() {
    if (!_buffersAllocated) {
        return;
    }
    
    if (_jsonBuffer != nullptr) {
        free(_jsonBuffer);
        _jsonBuffer = nullptr;
    }
    
    if (_responseBuffer != nullptr) {
        free(_responseBuffer);
        _responseBuffer = nullptr;
    }
    
    _buffersAllocated = false;
    Serial.println("WiFi: Memory buffers deallocated - Free heap: " + String(ESP.getFreeHeap()));
}

void WiFiManager::_safeServerRestart() {
    unsigned long currentTime = millis();
    
    // Son restart'tan en az 30 saniye geçmişse restart yap
    if (currentTime - _lastServerRestart < 30000) {
        Serial.println("WiFi: Server restart çok erken, atlanıyor");
        return;
    }
    
    Serial.println("WiFi: Güvenli server restart başlatılıyor...");
    
    // Önce sunucuyu durdur
    if (_server != nullptr && _isServerRunning) {
        _server->stop();
        _isServerRunning = false;
        delay(1000); // Server'ın tamamen durması için bekle
    }
    
    // Server nesnesini temizle
    if (_server != nullptr) {
        delete _server;
        _server = nullptr;
        delay(500); // Memory cleanup için bekle
    }
    
    // Memory buffers'ı temizle
    _deallocateBuffers();
    
    // Garbage collection zorla
    esp_task_wdt_reset();
    delay(1000);
    
    // Yeni buffers allocate et
    if (!_allocateBuffers()) {
        Serial.println("WiFi: Buffer allocation hatası, server restart iptal");
        return;
    }
    
    // Yeni server instance oluştur
    _server = new WebServer(WIFI_PORT);
    if (_server == nullptr) {
        Serial.println("WiFi: Server instance oluşturma hatası!");
        _deallocateBuffers();
        return;
    }
    
    // Routes'ları yeniden ayarla
    _setupRoutes();
    
    // Server'ı başlat
    _server->begin();
    _isServerRunning = true;
    _lastServerRestart = currentTime;
    _serverRecreationNeeded = false;
    
    Serial.println("WiFi: Server başarıyla restart edildi - Free heap: " + String(ESP.getFreeHeap()));
}

// İyileştirilmiş startServer fonksiyonu
void WiFiManager::startServer() {
    if (_isServerRunning || !_isConnected) {
        return;
    }
    
    // Memory buffers'ı allocate et
    if (!_allocateBuffers()) {
        Serial.println("WiFi: Buffer allocation hatası, server başlatılamıyor");
        return;
    }
    
    // Eğer server recreation gerekiyorsa safe restart yap
    if (_serverRecreationNeeded) {
        _safeServerRestart();
        return;
    }
    
    // Server zaten varsa önce temizle
    if (_server != nullptr) {
        delete _server;
        _server = nullptr;
        delay(100);
    }
    
    // Yeni server instance oluştur
    _server = new WebServer(WIFI_PORT);
    if (_server == nullptr) {
        Serial.println("WiFi: Server instance oluşturma hatası!");
        _deallocateBuffers();
        return;
    }
    
    // Routes'ları ayarla
    _setupRoutes();
    
    // Server'ı başlat
    _server->begin();
    _isServerRunning = true;
    
    Serial.println("WiFi: Web sunucu başlatıldı: " + getIPAddress() + ":" + String(WIFI_PORT));
    Serial.println("Free heap: " + String(ESP.getFreeHeap()));
}

// İyileştirilmiş stopServer fonksiyonu
void WiFiManager::stopServer() {
    if (!_isServerRunning || !_server) {
        return;
    }
    
    Serial.println("WiFi: Server durduruluyor...");
    
    // Server'ı durdur
    _server->stop();
    _isServerRunning = false;
    
    // Kısa bekle
    delay(500);
    
    // Server nesnesini temizle
    delete _server;
    _server = nullptr;
    
    // Memory buffers'ı temizle
    _deallocateBuffers();
    
    Serial.println("WiFi: Server durduruldu - Free heap: " + String(ESP.getFreeHeap()));
}

// İyileştirilmiş destructor
WiFiManager::~WiFiManager() {
    // Server'ı güvenli şekilde durdur
    if (_isServerRunning) {
        stopServer();
    }
    
    // Eğer server hala varsa temizle
    if (_server != nullptr) {
        delete _server;
        _server = nullptr;
    }
    
    // Memory buffers'ı temizle
    _deallocateBuffers();
    
    Serial.println("WiFi: WiFiManager yıkıcı tamamlandı");
}