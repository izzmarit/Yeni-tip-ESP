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
        // Storage yoksa varsayılan AP modunda başlat
        return beginAP();
    }
    
    WiFiConnectionMode mode = _storage->getWifiMode();
    
    if (mode == WIFI_CONN_MODE_STATION) {
        _stationSSID = _storage->getStationSSID();
        _stationPassword = _storage->getStationPassword();
        
        if (_stationSSID.length() > 0) {
            Serial.println("WiFi Manager: Station modunda başlatılıyor...");
            return beginStation(_stationSSID, _stationPassword);
        } else {
            Serial.println("WiFi Manager: Station SSID boş, Station modunda kalıyor...");
            // AP moduna GEÇMİYORUZ, station modunda kalıyoruz
            _connectionStatus = WIFI_STATUS_DISCONNECTED;
            return true; // Başarılı başlatma sayılır
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
    WiFi.disconnect(true);
    delay(1000);
    esp_task_wdt_reset();
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    
    _isConnected = true;
    _connectionStatus = WIFI_STATUS_AP_MODE;
    _ssid = AP_SSID;
    _password = AP_PASS;
    
    Serial.println("AP modu aktif: " + WiFi.softAPIP().toString());
    return true;
}

bool WiFiManager::beginStation(const String& ssid, const String& password) {
    _stationSSID = ssid;
    _stationPassword = password;
    
    WiFi.disconnect(true);
    delay(1000);
    esp_task_wdt_reset();
    
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);  // Otomatik yeniden bağlanmayı etkinleştir
    WiFi.setAutoConnect(true);     // Otomatik bağlanmayı etkinleştir
    WiFi.begin(ssid.c_str(), password.c_str());
    
    _connectionStatus = WIFI_STATUS_CONNECTING;
    _lastConnectionAttempt = millis();
    
    int timeout = 30;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        esp_task_wdt_reset();
        timeout--;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        _isConnected = true;
        _connectionStatus = WIFI_STATUS_CONNECTED;
        _ssid = ssid;
        _password = password;
        
        if (_storage != nullptr) {
            _storage->setWifiMode(WIFI_CONN_MODE_STATION);
            _storage->setStationSSID(ssid);
            _storage->setStationPassword(password);
            _storage->queueSave();
        }
        
        Serial.println("Station modunda bağlantı başarılı: " + WiFi.localIP().toString());
        return true;
    }
    
    // Bağlantı başarısız olsa bile station modunda kal
    _connectionStatus = WIFI_STATUS_FAILED;
    _isConnected = false;
    Serial.println("Station modunda bağlantı başarısız, tekrar denenecek...");
    return true; // Station modu aktif
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
    
    // Önce mevcut ayarları kaydet
    if (_storage != nullptr) {
        _storage->saveStateNow(); // Mode değişiminden önce tüm verileri kaydet
    }
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    if (_stationSSID.length() == 0) {
        Serial.println("Station SSID ayarlanmamış!");
        return false;
    }
    
    // Server'ı güvenli şekilde durdur
    if (_isServerRunning) {
        stopServer();
        delay(1000);
        esp_task_wdt_reset();
    }
    
    // WiFi modunu değiştir
    bool connected = beginStation(_stationSSID, _stationPassword);
    esp_task_wdt_reset();
    
    // Server'ı yeniden başlat
    delay(500);
    startServer();
    esp_task_wdt_reset();
    
    // Yeni modu kaydet
    if (_storage != nullptr) {
        _storage->setWifiMode(WIFI_CONN_MODE_STATION);
        _storage->saveStateNow(); // Mode değişiminden sonra da kaydet
    }
    
    Serial.println("WiFi: Station moduna geçiş tamamlandı");
    return true;
}

bool WiFiManager::switchToAPMode() {
    Serial.println("WiFi: AP moduna geçiş başlıyor...");
    
    // Önce mevcut ayarları kaydet
    if (_storage != nullptr) {
        _storage->saveStateNow(); // Mode değişiminden önce tüm verileri kaydet
    }
    
    // Watchdog besleme
    esp_task_wdt_reset();
    
    // Server'ı güvenli şekilde durdur
    if (_isServerRunning) {
        stopServer();
        delay(1000);
        esp_task_wdt_reset();
    }
    
    // WiFi modunu değiştir
    bool success = beginAP();
    esp_task_wdt_reset();
    
    // Server'ı yeniden başlat
    if (success) {
        delay(500);
        startServer();
        esp_task_wdt_reset();
        
        // Yeni modu kaydet
        if (_storage != nullptr) {
            _storage->setWifiMode(WIFI_CONN_MODE_AP);
            _storage->saveStateNow(); // Mode değişiminden sonra da kaydet
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
                                 int actualDay) {
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
    
    if (doc.containsKey("targetTemp")) {
        float targetTemp = doc["targetTemp"];
        _processParameterUpdate("targetTemp", String(targetTemp));
        Serial.println("App'den hedef sıcaklık güncellendi: " + String(targetTemp));
    }
    
    if (doc.containsKey("targetHumid")) {
        float targetHumid = doc["targetHumid"];
        _processParameterUpdate("targetHumid", String(targetHumid));
        Serial.println("App'den hedef nem güncellendi: " + String(targetHumid));
    }
    
    if (doc.containsKey("pidKp")) {
        _processParameterUpdate("pidKp", String((float)doc["pidKp"]));
    }
    if (doc.containsKey("pidKi")) {
        _processParameterUpdate("pidKi", String((float)doc["pidKi"]));
    }
    if (doc.containsKey("pidKd")) {
        _processParameterUpdate("pidKd", String((float)doc["pidKd"]));
    }
    if (doc.containsKey("pidMode")) {
        _processParameterUpdate("pidMode", String((int)doc["pidMode"]));
    }
    
    if (doc.containsKey("incubationType")) {
        _processParameterUpdate("incubationType", String((int)doc["incubationType"]));
    }
    if (doc.containsKey("isIncubationRunning")) {
        _processParameterUpdate("isIncubationRunning", String((bool)doc["isIncubationRunning"] ? "1" : "0"));
    }
    
    if (doc.containsKey("motorWaitTime")) {
        _processParameterUpdate("motorWaitTime", String((int)doc["motorWaitTime"]));
    }
    if (doc.containsKey("motorRunTime")) {
        _processParameterUpdate("motorRunTime", String((int)doc["motorRunTime"]));
    }
    
    if (doc.containsKey("alarmEnabled")) {
        _processParameterUpdate("alarmEnabled", String((bool)doc["alarmEnabled"] ? "1" : "0"));
        Serial.println("App'den alarm durumu güncellendi: " + String((bool)doc["alarmEnabled"] ? "AÇIK" : "KAPALI"));
    }
    if (doc.containsKey("tempLowAlarm")) {
        _processParameterUpdate("tempLowAlarm", String((float)doc["tempLowAlarm"]));
    }
    if (doc.containsKey("tempHighAlarm")) {
        _processParameterUpdate("tempHighAlarm", String((float)doc["tempHighAlarm"]));
    }
    if (doc.containsKey("humidLowAlarm")) {
        _processParameterUpdate("humidLowAlarm", String((float)doc["humidLowAlarm"]));
    }
    if (doc.containsKey("humidHighAlarm")) {
        _processParameterUpdate("humidHighAlarm", String((float)doc["humidHighAlarm"]));
    }
    
    if (doc.containsKey("manualDevTemp")) {
        _processParameterUpdate("manualDevTemp", String((float)doc["manualDevTemp"]));
    }
    if (doc.containsKey("manualHatchTemp")) {
        _processParameterUpdate("manualHatchTemp", String((float)doc["manualHatchTemp"]));
    }
    if (doc.containsKey("manualDevHumid")) {
        _processParameterUpdate("manualDevHumid", String((int)doc["manualDevHumid"]));
    }
    if (doc.containsKey("manualHatchHumid")) {
        _processParameterUpdate("manualHatchHumid", String((int)doc["manualHatchHumid"]));
    }
    if (doc.containsKey("manualDevDays")) {
        _processParameterUpdate("manualDevDays", String((int)doc["manualDevDays"]));
    }
    if (doc.containsKey("manualHatchDays")) {
        _processParameterUpdate("manualHatchDays", String((int)doc["manualHatchDays"]));
    }
    
    if (doc.containsKey("tempCalibration1")) {
        _processParameterUpdate("tempCalibration1", String((float)doc["tempCalibration1"]));
    }
    if (doc.containsKey("tempCalibration2")) {
        _processParameterUpdate("tempCalibration2", String((float)doc["tempCalibration2"]));
    }
    if (doc.containsKey("humidCalibration1")) {
        _processParameterUpdate("humidCalibration1", String((float)doc["humidCalibration1"]));
    }
    if (doc.containsKey("humidCalibration2")) {
        _processParameterUpdate("humidCalibration2", String((float)doc["humidCalibration2"]));
    }
    
    if (doc.containsKey("wifiStationSSID")) {
        _processParameterUpdate("wifiStationSSID", String((const char*)doc["wifiStationSSID"]));
    }
    if (doc.containsKey("wifiStationPassword")) {
        _processParameterUpdate("wifiStationPassword", String((const char*)doc["wifiStationPassword"]));
    }
    if (doc.containsKey("wifiMode")) {
        _processParameterUpdate("wifiMode", String((int)doc["wifiMode"]));
    }
}

String WiFiManager::createAppData() {
    StaticJsonDocument<2048> doc;
    
    doc["temperature"] = _currentTemp;
    doc["humidity"] = _currentHumid;
    doc["heaterState"] = _heaterState;
    doc["humidifierState"] = _humidifierState;
    doc["motorState"] = _motorState;
    
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
    
    doc["alarmEnabled"] = _alarmEnabled;
    doc["tempLowAlarm"] = _tempLowAlarm;
    doc["tempHighAlarm"] = _tempHighAlarm;
    doc["humidLowAlarm"] = _humidLowAlarm;
    doc["humidHighAlarm"] = _humidHighAlarm;
    
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
    
    // Alarm verileri
    doc["alarmEnabled"] = _alarmEnabled;
    doc["tempLowAlarm"] = _tempLowAlarm;
    doc["tempHighAlarm"] = _tempHighAlarm;
    doc["humidLowAlarm"] = _humidLowAlarm;
    doc["humidHighAlarm"] = _humidHighAlarm;
    
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
        
        setStationCredentials(ssid, password);
        
        _server->send(200, "application/json", _createSuccessResponse());
        
        delay(1000);
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
        
        // Başarılı yanıt ile güncel değeri döndür
        StaticJsonDocument<200> response;
        response["status"] = "success";
        response["message"] = "Temperature updated";
        response["targetTemp"] = targetTemp;
        response["timestamp"] = millis();
        
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
        
        // Başarılı yanıt ile güncel değeri döndür
        StaticJsonDocument<200> response;
        response["status"] = "success";
        response["message"] = "Humidity updated";
        response["targetHumid"] = targetHumid;
        response["timestamp"] = millis();
        
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
    
    if (doc.containsKey("kp")) {
        float kp = doc["kp"];
        if (kp >= 0.0 && kp <= 100.0) {
            _processParameterUpdate("pidKp", String(kp));
            responseMessage += "Kp güncellendi: " + String(kp) + " ";
            hasValidParam = true;
        }
    }
    
    if (doc.containsKey("ki")) {
        float ki = doc["ki"];
        if (ki >= 0.0 && ki <= 50.0) {
            _processParameterUpdate("pidKi", String(ki));
            responseMessage += "Ki güncellendi: " + String(ki) + " ";
            hasValidParam = true;
        }
    }
    
    if (doc.containsKey("kd")) {
        float kd = doc["kd"];
        if (kd >= 0.0 && kd <= 10.0) {
            _processParameterUpdate("pidKd", String(kd));
            responseMessage += "Kd güncellendi: " + String(kd) + " ";
            hasValidParam = true;
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
            responseMessage += "PID Modu: " + modeStr + " ";
            hasValidParam = true;
            
            Serial.println("WiFi API: PID modu değiştirildi -> " + modeStr);
        } else {
            _server->send(400, "application/json", 
                         _createErrorResponse("Invalid PID mode (0-2)"));
            return;
        }
    }
    
    if (doc.containsKey("pidAction")) {
        String action = doc["pidAction"];
        
        if (action == "start_manual") {
            _processParameterUpdate("pidMode", "1");
            responseMessage += "Manuel PID başlatıldı ";
            hasValidParam = true;
        } else if (action == "start_auto") {
            _processParameterUpdate("pidMode", "2");
            responseMessage += "Otomatik PID başlatıldı ";
            hasValidParam = true;
        } else if (action == "stop") {
            _processParameterUpdate("pidMode", "0");
            responseMessage += "PID durduruldu ";
            hasValidParam = true;
        }
    }
    
    if (hasValidParam) {
        StaticJsonDocument<200> response;
        response["status"] = "success";
        response["message"] = responseMessage.length() > 0 ? responseMessage : "PID parametreleri güncellendi";
        
        String responseStr;
        serializeJson(response, responseStr);
        _server->send(200, "application/json", responseStr);
    } else {
        _server->send(400, "application/json", 
                     _createErrorResponse("No valid PID parameters"));
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
    StaticJsonDocument<100> doc;
    doc["status"] = "success";
    doc["message"] = "Parameter updated successfully";
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