/**
 * @file wifi_manager.cpp
 * @brief WiFi bağlantı ve web sunucu yönetimi uygulaması
 * @version 1.1
 */

#include "wifi_manager.h"

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
}

WiFiManager::~WiFiManager() {
    // Bellek sızıntısını önlemek için server nesnesini temizle
    if (_server != nullptr) {
        stopServer();
        delete _server;
        _server = nullptr;
    }
}

void WiFiManager::setStorage(Storage* storage) {
    _storage = storage;
}

bool WiFiManager::begin() {
    if (_storage == nullptr) {
        Serial.println("WiFi Manager: Storage referansı ayarlanmamış!");
        return beginAP(); // Fallback olarak AP modu
    }
    
    // Storage'dan WiFi ayarlarını yükle
    WiFiConnectionMode mode = _storage->getWifiMode();
    
    if (mode == WIFI_CONN_MODE_STATION) {
        // Station modunda başlat
        _stationSSID = _storage->getStationSSID();
        _stationPassword = _storage->getStationPassword();
        
        if (_stationSSID.length() > 0) {
            Serial.println("WiFi Manager: Station modunda başlatılıyor...");
            return beginStation(_stationSSID, _stationPassword);
        } else {
            Serial.println("WiFi Manager: Station SSID boş, AP moduna geçiliyor...");
            return beginAP();
        }
    } else {
        // AP modunda başlat
        Serial.println("WiFi Manager: AP modunda başlatılıyor...");
        return beginAP();
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
    
    _connectionStatus = WIFI_STATUS_CONNECTING;
    _lastConnectionAttempt = millis();
    
    // Bağlantı için 10 saniye bekle
    int timeout = 20; // 20 x 500ms = 10 saniye
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        esp_task_wdt_reset(); // Her bekleme adımında watchdog besleme
        timeout--;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        _isConnected = true;
        _connectionStatus = WIFI_STATUS_CONNECTED;
        Serial.println("WiFi bağlantısı başarılı: " + WiFi.localIP().toString());
        return true;
    }
    
    // Bağlantı başarısız, AP moduna geç
    _connectionStatus = WIFI_STATUS_FAILED;
    Serial.println("WiFi bağlantısı başarısız, AP moduna geçiliyor...");
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
    _connectionStatus = WIFI_STATUS_AP_MODE;
    _ssid = AP_SSID;
    _password = AP_PASS;
    
    Serial.println("AP modu aktif: " + WiFi.softAPIP().toString());
    return true;
}

bool WiFiManager::beginStation(const String& ssid, const String& password) {
    _stationSSID = ssid;
    _stationPassword = password;
    
    // Önce mevcut bağlantıları temizle
    WiFi.disconnect(true);
    delay(1000);
    esp_task_wdt_reset();
    
    // WiFi'yi STA (station) modunda başlat
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    _connectionStatus = WIFI_STATUS_CONNECTING;
    _lastConnectionAttempt = millis();
    
    // Bağlantı için 15 saniye bekle
    int timeout = 30; // 30 x 500ms = 15 saniye
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
        
        // Station modunu storage'a kaydet
        if (_storage != nullptr) {
            _storage->setWifiMode(WIFI_CONN_MODE_STATION);
            _storage->setStationSSID(ssid);
            _storage->setStationPassword(password);
            _storage->queueSave();
        }
        
        Serial.println("Station modunda bağlantı başarılı: " + WiFi.localIP().toString());
        return true;
    }
    
    // Bağlantı başarısız
    _connectionStatus = WIFI_STATUS_FAILED;
    Serial.println("Station modunda bağlantı başarısız");
    return false;
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
        return true; // AP modu her zaman "bağlı" olarak kabul edilir
    }
    
    return false;
}

WiFiConnectionStatus WiFiManager::getConnectionStatus() const {
    return _connectionStatus;
}

WiFiMode_t WiFiManager::getCurrentMode() const {
    return WiFi.getMode();
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
        Serial.println("Web sunucu başlatıldı: " + getIPAddress() + ":" + String(WIFI_PORT));
    }
}

void WiFiManager::stopServer() {
    if (!_isServerRunning || !_server) {
        return;
    }
    
    _server->end();
    _isServerRunning = false;
    Serial.println("Web sunucu durduruldu");
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

void WiFiManager::setStationCredentials(const String& ssid, const String& password) {
    _stationSSID = ssid;
    _stationPassword = password;
}

bool WiFiManager::switchToStationMode() {
    if (_stationSSID.length() == 0) {
        Serial.println("Station SSID ayarlanmamış!");
        return false;
    }
    
    // Sunucuyu durdur
    stopServer();
    
    // Station moduna geç
    bool success = beginStation(_stationSSID, _stationPassword);
    
    if (success) {
        // Sunucuyu yeniden başlat
        startServer();
        
        // Storage'a kaydet
        if (_storage != nullptr) {
            _storage->setWifiMode(WIFI_CONN_MODE_STATION);
            _storage->queueSave();
        }
    } else {
        // Başarısızsa AP moduna geri dön
        beginAP();
        startServer();
    }
    
    return success;
}

bool WiFiManager::switchToAPMode() {
    // Sunucuyu durdur
    stopServer();
    
    // AP moduna geç
    bool success = beginAP();
    
    if (success) {
        // Sunucuyu yeniden başlat
        startServer();
        
        // Storage'a kaydet
        if (_storage != nullptr) {
            _storage->setWifiMode(WIFI_CONN_MODE_AP);
            _storage->queueSave();
        }
    }
    
    return success;
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
    // Bağlantı durumunu kontrol et
    _checkConnectionStatus();
    
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
        if (WiFi.status() == WL_CONNECTED) {
            if (_connectionStatus != WIFI_STATUS_CONNECTED) {
                _connectionStatus = WIFI_STATUS_CONNECTED;
                _isConnected = true;
                Serial.println("WiFi bağlantısı yeniden kuruldu");
            }
        } else {
            if (_connectionStatus == WIFI_STATUS_CONNECTED) {
                _connectionStatus = WIFI_STATUS_DISCONNECTED;
                _isConnected = false;
                Serial.println("WiFi bağlantısı koptu");
            }
        }
    }
}

void WiFiManager::_scanWiFiNetworks() {
    WiFi.scanNetworks(true); // Asenkron tarama başlat
}

String WiFiManager::_getHtmlContent() {
    // WiFi ayar sayfası da dahil olmak üzere gelişmiş HTML
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
.nav { text-align: center; margin-bottom: 20px; }
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
</div>
</body>
</html>
)rawliteral";
    
    return FPSTR(HTML_TEMPLATE);
}

String WiFiManager::_getWiFiConfigHTML() {
    // WiFi ayar sayfası HTML'i
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
    
    data.networks.forEach(network => {
      const item = document.createElement('div');
      item.className = 'network-item';
      item.innerHTML = `<strong>${network.ssid}</strong> (${network.rssi} dBm)`;
      item.onclick = () => selectNetwork(network.ssid);
      networkList.appendChild(item);
    });
  })
  .catch(error => {
    console.error('Ağ listesi yüklenemedi:', error);
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
        WiFi.scanDelete(); // Tarama sonuçlarını temizle
    }
    
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
    
    // WiFi ayar sayfası
    _server->on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", _getWiFiConfigHTML());
    });
    
    // Durum verileri JSON API
    _server->on("/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", _getStatusJson());
    });
    
    // WiFi ağları listesi
    _server->on("/api/wifi/networks", HTTP_GET, [this](AsyncWebServerRequest *request) {
        // Tarama başlat
        WiFi.scanNetworks(true);
        // Biraz bekle
        delay(100);
        request->send(200, "application/json", _getWiFiNetworksJson());
    });
    
    // WiFi bağlantısı
    _server->on("/api/wifi/connect", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleWiFiConnect(request, data, len, index, total);
        }
    );
    
    // AP moduna geçiş
    _server->on("/api/wifi/ap", HTTP_POST, [this](AsyncWebServerRequest *request) {
        bool success = switchToAPMode();
        if (success) {
            request->send(200, "application/json", _createSuccessResponse());
        } else {
            request->send(500, "application/json", _createErrorResponse("AP modu başlatılamadı"));
        }
    });
    
    // Diğer API uç noktaları (eski fonksiyonlar)
    _server->on("/api/temperature", HTTP_POST, 
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetTemperature(request, data, len, index, total);
        }
    );
    
    _server->on("/api/humidity", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetHumidity(request, data, len, index, total);
        }
    );
    
    _server->on("/api/pid", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetPidParameters(request, data, len, index, total);
        }
    );
    
    _server->on("/api/motor", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetMotorSettings(request, data, len, index, total);
        }
    );
    
    _server->on("/api/alarm", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetAlarmSettings(request, data, len, index, total);
        }
    );
    
    _server->on("/api/calibration", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetCalibration(request, data, len, index, total);
        }
    );
    
    _server->on("/api/incubation", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _handleSetIncubationSettings(request, data, len, index, total);
        }
    );
}

void WiFiManager::_handleWiFiConnect(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index + len == total) {
        String jsonString = String((char*)data).substring(0, len);
        
        StaticJsonDocument<300> doc;
        DeserializationError error = deserializeJson(doc, jsonString);
        
        if (error) {
            request->send(400, "application/json", _createErrorResponse("Invalid JSON"));
            return;
        }
        
        if (doc.containsKey("ssid")) {
            String ssid = doc["ssid"];
            String password = doc["password"] | "";
            
            // Station moduna geçmeyi dene
            setStationCredentials(ssid, password);
            
            request->send(200, "application/json", _createSuccessResponse());
            
            // Bağlantıyı arka planda dene
            // Not: Bu işlem async olmalı, burada basit bir delay kullanıyoruz
            delay(1000);
            switchToStationMode();
            
        } else {
            request->send(400, "application/json", _createErrorResponse("Missing ssid parameter"));
        }
    }
}

// Diğer handle fonksiyonları (değişiklik yok, eski hallerini koruyoruz)
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