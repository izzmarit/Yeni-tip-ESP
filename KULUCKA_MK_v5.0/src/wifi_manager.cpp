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

WiFiManager::~WiFiManager() {
    // Bellek sızıntısını önlemek için server nesnesini temizle
    if (_server != nullptr) {
        stopServer();
        delete _server;
        _server = nullptr;
    }
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
    
    // Zaten bir server nesnesi varsa önce onu temizle
    if (_server != nullptr) {
        delete _server;
        _server = nullptr;
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
    // Statik HTML şablonu kullan
    static const char HTML_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
body { font-family: Arial; margin: 0; padding: 20px; }
.card { background-color: #f1f1f1; padding: 20px; margin-bottom: 15px; border-radius: 5px; }
.row:after { content: ''; display: table; clear: both; }
.column { float: left; width: 50%; }
h2 { color: #333; }
.temp { color: red; }
.humid { color: blue; }
.status { font-weight: bold; }
.active { color: green; }
.inactive { color: gray; }
</style>
<script>
function updateStatus() {
  fetch('/status').then(response => response.json()).then(data => {
    document.getElementById('temp').innerHTML = data.temperature.toFixed(1) + '&deg;C';
    document.getElementById('humid').innerHTML = data.humidity.toFixed(0) + '%';
    document.getElementById('targetTemp').innerHTML = data.targetTemp.toFixed(1) + '&deg;C';
    document.getElementById('targetHumid').innerHTML = data.targetHumid.toFixed(0) + '%';
    document.getElementById('day').innerHTML = data.currentDay + '/' + data.totalDays;
    document.getElementById('type').innerHTML = data.incubationType;
    document.getElementById('heater').className = data.heaterState ? 'status active' : 'status inactive';
    document.getElementById('heater').innerHTML = data.heaterState ? 'AÇIK' : 'KAPALI';
    document.getElementById('humidifier').className = data.humidifierState ? 'status active' : 'status inactive';
    document.getElementById('humidifier').innerHTML = data.humidifierState ? 'AÇIK' : 'KAPALI';
    document.getElementById('motor').className = data.motorState ? 'status active' : 'status inactive';
    document.getElementById('motor').innerHTML = data.motorState ? 'AÇIK' : 'KAPALI';
  });
  setTimeout(updateStatus, 2000);
}
document.addEventListener('DOMContentLoaded', updateStatus);
</script>
</head>
<body>
<h1>KULUÇKA MK v5.0</h1>
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
</div>
</body>
</html>
)rawliteral";
    
    return FPSTR(HTML_TEMPLATE);
}

String WiFiManager::_getStatusJson() {
    // Sabit boyutlu bir bellek ayır
    StaticJsonDocument<512> doc;
    
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
    
    // Sıcaklık ayarlama
    _server->on("/api/temperature", HTTP_POST, 
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetTemperature(request, data, len, index, total);
        }
    );
    
    // Nem ayarlama
    _server->on("/api/humidity", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetHumidity(request, data, len, index, total);
        }
    );
    
    // PID parametreleri
    _server->on("/api/pid", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetPidParameters(request, data, len, index, total);
        }
    );
    
    // Motor ayarları
    _server->on("/api/motor", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetMotorSettings(request, data, len, index, total);
        }
    );
    
    // Alarm ayarları
    _server->on("/api/alarm", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetAlarmSettings(request, data, len, index, total);
        }
    );
    
    // Kalibrasyon ayarları
    _server->on("/api/calibration", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetCalibration(request, data, len, index, total);
        }
    );
    
    // Kuluçka ayarları
    _server->on("/api/incubation", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetIncubationSettings(request, data, len, index, total);
        }
    );
}

void WiFiManager::_handleSetTemperature(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index + len == total) {
        String jsonString = String((char*)data).substring(0, len);
        
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        
        if (error) {
            request->send(400, "application/json", _createErrorResponse("Invalid JSON"));
            return;
        }
        
        if (doc.containsKey("targetTemp")) {
            float targetTemp = doc["targetTemp"];
            _processParameterUpdate("targetTemp", String(targetTemp));
            request->send(200, "application/json", _createSuccessResponse());
        } else {
            request->send(400, "application/json", _createErrorResponse("Missing targetTemp parameter"));
        }
    }
}

void WiFiManager::_handleSetHumidity(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index + len == total) {
        String jsonString = String((char*)data).substring(0, len);
        
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        
        if (error) {
            request->send(400, "application/json", _createErrorResponse("Invalid JSON"));
            return;
        }
        
        if (doc.containsKey("targetHumid")) {
            float targetHumid = doc["targetHumid"];
            _processParameterUpdate("targetHumid", String(targetHumid));
            request->send(200, "application/json", _createSuccessResponse());
        } else {
            request->send(400, "application/json", _createErrorResponse("Missing targetHumid parameter"));
        }
    }
}

void WiFiManager::_handleSetPidParameters(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index + len == total) {
        String jsonString = String((char*)data).substring(0, len);
        
        StaticJsonDocument<300> doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        
        if (error) {
            request->send(400, "application/json", _createErrorResponse("Invalid JSON"));
            return;
        }
        
        bool hasValidParam = false;
        
        if (doc.containsKey("kp")) {
            _processParameterUpdate("pidKp", String((float)doc["kp"]));
            hasValidParam = true;
        }
        
        if (doc.containsKey("ki")) {
            _processParameterUpdate("pidKi", String((float)doc["ki"]));
            hasValidParam = true;
        }
        
        if (doc.containsKey("kd")) {
            _processParameterUpdate("pidKd", String((float)doc["kd"]));
            hasValidParam = true;
        }
        
        if (hasValidParam) {
            request->send(200, "application/json", _createSuccessResponse());
        } else {
            request->send(400, "application/json", _createErrorResponse("No valid PID parameters"));
        }
    }
}

void WiFiManager::_handleSetMotorSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index + len == total) {
        String jsonString = String((char*)data).substring(0, len);
        
        StaticJsonDocument<300> doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        
        if (error) {
            request->send(400, "application/json", _createErrorResponse("Invalid JSON"));
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
            request->send(200, "application/json", _createSuccessResponse());
        } else {
            request->send(400, "application/json", _createErrorResponse("No valid motor parameters"));
        }
    }
}

void WiFiManager::_handleSetAlarmSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index + len == total) {
        String jsonString = String((char*)data).substring(0, len);
        
        StaticJsonDocument<400> doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        
        if (error) {
            request->send(400, "application/json", _createErrorResponse("Invalid JSON"));
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
        
        if (hasValidParam) {
            request->send(200, "application/json", _createSuccessResponse());
        } else {
            request->send(400, "application/json", _createErrorResponse("No valid alarm parameters"));
        }
    }
}

void WiFiManager::_handleSetCalibration(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index + len == total) {
        String jsonString = String((char*)data).substring(0, len);
        
        StaticJsonDocument<400> doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        
        if (error) {
            request->send(400, "application/json", _createErrorResponse("Invalid JSON"));
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
            request->send(200, "application/json", _createSuccessResponse());
        } else {
            request->send(400, "application/json", _createErrorResponse("No valid calibration parameters"));
        }
    }
}

void WiFiManager::_processParameterUpdate(const String& param, const String& value) {
    // main.cpp'deki handleWifiParameterUpdate fonksiyonunu çağır
    extern void handleWifiParameterUpdate(String param, String value);
    handleWifiParameterUpdate(param, value);
}

void WiFiManager::_handleSetIncubationSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index + len == total) {
        String jsonString = String((char*)data).substring(0, len);
        
        StaticJsonDocument<500> doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        
        if (error) {
            request->send(400, "application/json", _createErrorResponse("Invalid JSON"));
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
            request->send(200, "application/json", _createSuccessResponse());
        } else {
            request->send(400, "application/json", _createErrorResponse("No valid incubation parameters"));
        }
    }
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