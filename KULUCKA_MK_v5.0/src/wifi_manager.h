## wifi_manager.h

```cpp
/**
 * @file wifi_manager.h
 * @brief WiFi bağlantı ve web sunucu yönetimi
 * @version 1.2
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include "config.h"
#include "storage.h"

// WiFi bağlantı durumları
enum WiFiConnectionStatus {
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_FAILED,
    WIFI_STATUS_AP_MODE
};

class WiFiManager {
public:
    // Yapılandırıcı
    WiFiManager();
    
    // Yıkıcı - bellek temizleme için
    ~WiFiManager();
    
    // WiFi yönetimini başlat (storage'dan ayarları oku)
    bool begin();
    
    // WiFi yönetimini başlat
    bool begin(const String& ssid, const String& password);
    
    // AP modunda başlat
    bool beginAP();
    
    // Station modunda başlat
    bool beginStation(const String& ssid, const String& password);
    
    // WiFi bağlantısını durdur
    void stop();
    
    // WiFi bağlandı mı?
    bool isConnected() const;
    
    // WiFi durumunu al
    WiFiConnectionStatus getConnectionStatus() const;
    
    // Mevcut WiFi modunu al
    WiFiMode_t getCurrentMode() const;
    
    // Web sunucuyu başlat
    void startServer();
    
    // Web sunucuyu durdur
    void stopServer();
    
    // Web sunucu çalışıyor mu?
    bool isServerRunning() const;
    
    // IP adresini al
    String getIPAddress() const;
    
    // SSID'yi al
    String getSSID() const;
    
    // WiFi sinyal gücünü al
    int getSignalStrength() const;
    
    // Station modunda bağlanacak ağı ayarla
    void setStationCredentials(const String& ssid, const String& password);
    
    // Station moduna geç
    bool switchToStationMode();
    
    // AP moduna geç
    bool switchToAPMode();
    
    // WiFi ayarlarını storage'a kaydet
    void saveWiFiSettings();
    
    // Durum verilerini güncelle
    void updateStatusData(float temperature, float humidity, bool heaterState, 
                        bool humidifierState, bool motorState, int currentDay, 
                        int totalDays, String incubationType, float targetTemp, 
                        float targetHumidity);
    
    // Komutları ve yeni ayarları işle
    void handleRequests();
    
    // Telefon uygulamasından gelen verileri işle
    void processAppData(String jsonData);
    
    // Telefon uygulamasına gönderilecek verileri oluştur
    String createAppData();
    
    // WiFi durumu string'i al
    String getStatusString() const;
    
    // Storage referansını ayarla
    void setStorage(Storage* storage);

private:
    // WiFi ve web sunucu değişkenleri
    AsyncWebServer* _server;
    bool _isConnected;
    bool _isServerRunning;
    String _ssid;
    String _password;
    String _stationSSID;
    String _stationPassword;
    WiFiConnectionStatus _connectionStatus;
    Storage* _storage;
    
    // Durum verileri
    float _currentTemp;
    float _currentHumid;
    bool _heaterState;
    bool _humidifierState;
    bool _motorState;
    int _currentDay;
    int _totalDays;
    String _incubationType;
    float _targetTemp;
    float _targetHumid;
    
    // **EKLEME: PID ve alarm durum verileri**
    int _pidMode;
    float _pidKp;
    float _pidKi;
    float _pidKd;
    bool _alarmEnabled;
    float _tempLowAlarm;
    float _tempHighAlarm;
    float _humidLowAlarm;
    float _humidHighAlarm;
    
    // Bağlantı yönetimi
    unsigned long _lastConnectionAttempt;
    static const unsigned long CONNECTION_TIMEOUT = 10000; // 10 saniye
    
    // Web sunucu için HTML içeriği
    String _getHtmlContent();
    
    // WiFi ayar sayfası HTML içeriği
    String _getWiFiConfigHTML();
    
    // Durum verilerini JSON olarak al
    String _getStatusJson();
    
    // WiFi ağları listesini JSON olarak al
    String _getWiFiNetworksJson();
    
    // API uç noktalarını ayarla
    void _setupRoutes();
    
    // API istek işleyicileri
    void _handleRoot(AsyncWebServerRequest *request);
    void _handleGetStatus(AsyncWebServerRequest *request);
    void _handleSetParameter(AsyncWebServerRequest *request);
    void _handleSetTemperature(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void _handleSetHumidity(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void _handleSetPidParameters(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void _handleSetMotorSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void _handleSetAlarmSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void _handleSetCalibration(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void _handleSetIncubationSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void _handleWiFiConfig(AsyncWebServerRequest *request);
    void _handleWiFiNetworks(AsyncWebServerRequest *request);
    void _handleWiFiConnect(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    
    // JSON işleme yardımcı fonksiyonları
    void _processParameterUpdate(const String& param, const String& value);
    String _createSuccessResponse();
    String _createErrorResponse(const String& message);
    
    // Bağlantı durumunu kontrol et
    void _checkConnectionStatus();
    
    // WiFi ağlarını tara
    void _scanWiFiNetworks();
};

#endif // WIFI_MANAGER_H