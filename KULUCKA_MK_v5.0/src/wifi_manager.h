/**
 * @file wifi_manager.h
 * @brief WiFi bağlantı ve web sunucu yönetimi
 * @version 1.0
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include "config.h"

class WiFiManager {
public:
    // Yapılandırıcı
    WiFiManager();
    
    // WiFi yönetimini başlat
    bool begin(const String& ssid, const String& password);
    
    // AP modunda başlat
    bool beginAP();
    
    // WiFi bağlantısını durdur
    void stop();
    
    // WiFi bağlandı mı?
    bool isConnected() const;
    
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

private:
    // WiFi ve web sunucu değişkenleri
    AsyncWebServer* _server;
    bool _isConnected;
    bool _isServerRunning;
    String _ssid;
    String _password;
    
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
    
    // Web sunucu için HTML içeriği
    String _getHtmlContent();
    
    // Durum verilerini JSON olarak al
    String _getStatusJson();
    
    // API uç noktalarını ayarla
    void _setupRoutes();
    
    // API istek işleyicileri
    void _handleRoot(AsyncWebServerRequest *request);
    void _handleGetStatus(AsyncWebServerRequest *request);
    void _handleSetParameter(AsyncWebServerRequest *request);
};

#endif // WIFI_MANAGER_H