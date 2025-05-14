/**
 * @file wifi_manager.cpp
 * @brief WiFi bağlantı ve web sunucu yönetimi uygulaması
 * @version 1.0
 */

#include "wifi_manager.h"

WiFiManager::WiFiManager() {
    _server = nullptr;
    _isConnected = false;
    _isServerRunning = false;
    _ssid = "";
    _password = "";
    
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
}

bool WiFiManager::begin(const String& ssid, const String& password) {
    _ssid = ssid;
    _password = password;
    
    // Önce mevcut bağlantıları temizle
    WiFi.disconnect(true);
    delay(1000);
    esp_task_wdt_reset(); // Uzun süren işlemler için watchdog besleme
    
    // WiFi'yi STA (station) modunda başlat
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    // Bağlantı için 10 saniye bekle
    int timeout = 20; // 20 x 500ms = 10 saniye
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        esp_task_wdt_reset(); // Her bekleme adımında watchdog besleme
        timeout--;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        _isConnected = true;
        return true;
    }
    
    // Bağlantı başarısız, AP moduna geç
    return beginAP();
}

bool WiFiManager::beginAP() {
    // WiFi'yi AP (access point) modunda başlat
    WiFi.disconnect(true);
    delay(1000);
    esp_task_wdt_reset(); // Uzun süren işlemler için watchdog besleme
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    
    _isConnected = true;
    _ssid = AP_SSID;
    _password = AP_PASS;
    
    return true;
}

void WiFiManager::stop() {
    if (_isServerRunning) {
        stopServer();
    }
    
    WiFi.disconnect(true);
    _isConnected = false;
}

bool WiFiManager::isConnected() const {
    if (WiFi.getMode() == WIFI_STA) {
        return WiFi.status() == WL_CONNECTED;
    } else if (WiFi.getMode() == WIFI_AP) {
        return true; // AP modu her zaman "bağlı" olarak kabul edilir
    }
    
    return false;
}

void WiFiManager::startServer() {
    if (_isServerRunning || !_isConnected) {
        return;
    }
    
    // Web sunucuyu oluştur
    _server = new AsyncWebServer(WIFI_PORT);
    
    if (_server) {
        // API uç noktalarını ayarla
        _setupRoutes();
        
        // Sunucuyu başlat
        _server->begin();
        _isServerRunning = true;
    }
}

void WiFiManager::stopServer() {
    if (!_isServerRunning || !_server) {
        return;
    }
    
    _server->end();
    delete _server;
    _server = nullptr;
    _isServerRunning = false;
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
    
    return 0; // AP modunda sinyal gücü ölçülmez
}

void WiFiManager::updateStatusData(float temperature, float humidity, bool heaterState, 
                                 bool humidifierState, bool motorState, int currentDay, 
                                 int totalDays, String incubationType, float targetTemp, 
                                 float targetHumidity) {
    _currentTemp = temperature;
    _currentHumid = humidity;
    _heaterState = heaterState;
    _humidifierState = humidifierState;
    _motorState = motorState;
    _currentDay = currentDay;
    _totalDays = totalDays;
    _incubationType = incubationType;
    _targetTemp = targetTemp;
    _targetHumid = targetHumidity;
}

void WiFiManager::handleRequests() {
    // AsyncWebServer zaten arka planda çalışıyor, 
    // burada loop() içinde herhangi bir işlem yapmaya gerek yok
}

void WiFiManager::processAppData(String jsonData) {
    // JSON verisini ayrıştır
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, jsonData);
    
    // Veriler burada işlenecek
    // (Telefon uygulamasından gelen komutlar, vb.)
}

String WiFiManager::createAppData() {
    // Durum verilerini JSON olarak oluştur
    return _getStatusJson();
}

String WiFiManager::_getHtmlContent() {
    // Basit bir web arayüzü HTML içeriği
    String html = "<!DOCTYPE html><html>";
    html += "<head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial; margin: 0; padding: 20px; }";
    html += ".card { background-color: #f1f1f1; padding: 20px; margin-bottom: 15px; border-radius: 5px; }";
    html += ".row:after { content: ''; display: table; clear: both; }";
    html += ".column { float: left; width: 50%; }";
    html += "h2 { color: #333; }";
    html += ".temp { color: red; }";
    html += ".humid { color: blue; }";
    html += ".status { font-weight: bold; }";
    html += ".active { color: green; }";
    html += ".inactive { color: gray; }";
    html += "</style>";
    html += "<script>";
    html += "function updateStatus() {";
    html += "  fetch('/status').then(response => response.json()).then(data => {";
    html += "    document.getElementById('temp').innerHTML = data.temperature.toFixed(1) + '&deg;C';";
    html += "    document.getElementById('humid').innerHTML = data.humidity.toFixed(0) + '%';";
    html += "    document.getElementById('targetTemp').innerHTML = data.targetTemp.toFixed(1) + '&deg;C';";
    html += "    document.getElementById('targetHumid').innerHTML = data.targetHumid.toFixed(0) + '%';";
    html += "    document.getElementById('day').innerHTML = data.currentDay + '/' + data.totalDays;";
    html += "    document.getElementById('type').innerHTML = data.incubationType;";
    html += "    document.getElementById('heater').className = data.heaterState ? 'status active' : 'status inactive';";
    html += "    document.getElementById('heater').innerHTML = data.heaterState ? 'AÇIK' : 'KAPALI';";
    html += "    document.getElementById('humidifier').className = data.humidifierState ? 'status active' : 'status inactive';";
    html += "    document.getElementById('humidifier').innerHTML = data.humidifierState ? 'AÇIK' : 'KAPALI';";
    html += "    document.getElementById('motor').className = data.motorState ? 'status active' : 'status inactive';";
    html += "    document.getElementById('motor').innerHTML = data.motorState ? 'AÇIK' : 'KAPALI';";
    html += "  });";
    html += "  setTimeout(updateStatus, 2000);"; // 2 saniyede bir güncelle
    html += "}";
    html += "document.addEventListener('DOMContentLoaded', updateStatus);";
    html += "</script>";
    html += "</head>";
    html += "<body>";
    html += "<h1>KULUÇKA MK v5.0</h1>";
    
    // Sıcaklık ve Nem kartı
    html += "<div class='card'>";
    html += "<h2>Sıcaklık ve Nem</h2>";
    html += "<div class='row'>";
    html += "<div class='column'>";
    html += "<h3>Sıcaklık: <span id='temp' class='temp'>--.-&deg;C</span></h3>";
    html += "<p>Hedef: <span id='targetTemp'>--.-&deg;C</span></p>";
    html += "<p>Isıtıcı: <span id='heater' class='status'>--</span></p>";
    html += "</div>";
    html += "<div class='column'>";
    html += "<h3>Nem: <span id='humid' class='humid'>--%</span></h3>";
    html += "<p>Hedef: <span id='targetHumid'>--%</span></p>";
    html += "<p>Nemlendirici: <span id='humidifier' class='status'>--</span></p>";
    html += "</div>";
    html += "</div>";
    html += "</div>";
    
    // Kuluçka bilgileri kartı
    html += "<div class='card'>";
    html += "<h2>Kuluçka Durumu</h2>";
    html += "<div class='row'>";
    html += "<div class='column'>";
    html += "<h3>Gün: <span id='day'>--/--</span></h3>";
    html += "<p>Tip: <span id='type'>--</span></p>";
    html += "</div>";
    html += "<div class='column'>";
    html += "<h3>Motor: <span id='motor' class='status'>--</span></h3>";
    html += "</div>";
    html += "</div>";
    html += "</div>";
    
    html += "</body></html>";
    
    return html;
}

String WiFiManager::_getStatusJson() {
    // Durum verilerini JSON olarak oluştur
    DynamicJsonDocument doc(1024);
    
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
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    return jsonString;
}

void WiFiManager::_setupRoutes() {
    if (!_server) {
        return;
    }
    
    // Kök sayfa - web arayüzü
    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", _getHtmlContent());
    });
    
    // Durum verileri JSON API
    _server->on("/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", _getStatusJson());
    });
    
    // JSON formatında parametre ayarlamaları için API
    _server->on("/api/set", HTTP_POST, [this](AsyncWebServerRequest *request) {
        // Burada POST verilerini işlemek için body parser gerekiyor
        // AsyncWebServer'ın JSON body parser özelliğini kullanabiliriz
        // Ancak bu örnek için basit bir form POST işlemi ile sınırlı kalacak
        if (request->hasParam("param", true) && request->hasParam("value", true)) {
            String param = request->getParam("param", true)->value();
            String value = request->getParam("value", true)->value();
            
            // Parametre ve değeri işle
            request->send(200, "application/json", "{\"status\":\"success\"}");
        } else {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing parameters\"}");
        }
    });
}