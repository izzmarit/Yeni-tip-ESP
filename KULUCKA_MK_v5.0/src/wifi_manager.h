/**
 * @file wifi_manager.h
 * @brief WiFi bağlantı ve web sunucu yönetimi (WebServer ile)
 * @version 1.5 - Compile hataları düzeltildi ve Android uygulaması entegrasyonu tamamlandı
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
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

// Forward declarations
class Storage;
class Relays;
class Incubation;
class Sensors;
class AlarmManager;
class RTCModule;
class PIDController;
class OTAManager;

class WiFiManager {
public:
    // Yapılandırıcı
    WiFiManager();
    
    // Yıkıcı - bellek temizleme için
    ~WiFiManager();

    // Sabit IP ayarlama
    void setStaticIP(bool useStatic, IPAddress ip, IPAddress gateway, IPAddress subnet, IPAddress dns);
    
    // mDNS adını ayarla
    void setMDNSName(const String& name) { _mdnsName = name; }
    
    // Ağ bilgilerini al
    String getNetworkInfo() const;
    
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
    
    // Durum verilerini güncelle - GENİŞLETİLMİŞ VERSİYON
    void updateStatusData(float temperature, float humidity, bool heaterState, 
                     bool humidifierState, bool motorState, int currentDay, 
                     int totalDays, String incubationType, float targetTemp, 
                     float targetHumidity, bool isIncubationCompleted = false,
                     int actualDay = 0,
                     float temp1 = 0.0, float temp2 = 0.0,
                     float humid1 = 0.0, float humid2 = 0.0,
                     bool sensor1Working = false, bool sensor2Working = false);
    
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

    // PID Mode güncelleme fonksiyonu
    void setPidMode(int mode);

    // Server kontrol fonksiyonları
    void handleClient();
    void handleConfiguration();

private:
    // WiFi ve web sunucu değişkenleri
    WebServer* _server;
    bool _isConnected;
    bool _isServerRunning;
    String _ssid;
    String _password;
    String _stationSSID;
    String _stationPassword;
    WiFiConnectionStatus _connectionStatus;
    Storage* _storage;

    // Server yaşam döngüsü yönetimi - YENİ EKLENECEK
    bool _serverInitialized;
    unsigned long _lastServerCheck;
    unsigned long _serverUptime;
    uint16_t _serverRestartCount;
    
    // Bellek koruma mekanizması - YENİ EKLENECEK
    uint32_t _lastFreeHeap;
    uint32_t _minFreeHeap;
    bool _memoryProtectionActive;

    // mDNS ve network ayarları
    String _mdnsName;
    bool _useStaticIP;
    IPAddress _staticIP;
    IPAddress _gateway;
    IPAddress _subnet;
    IPAddress _dns;
    
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
    bool _isIncubationRunning;
    
    // YENİ: Kuluçka tamamlanma durumu ve gerçek gün sayısı
    bool _isIncubationCompleted;
    int _actualDay;
    
    // PID ve alarm durum verileri
    int _pidMode;
    float _pidKp;
    float _pidKi;
    float _pidKd;
    bool _alarmEnabled;
    float _tempLowAlarm;
    float _tempHighAlarm;
    float _humidLowAlarm;
    float _humidHighAlarm;

    // Sensör detaylı verileri
    float _temp1;
    float _temp2;
    float _humid1;
    float _humid2;
    bool _sensor1Working;
    bool _sensor2Working;
    
    // Motor ayarları
    uint32_t _motorWaitTime;
    uint32_t _motorRunTime;
    
    // Kalibrasyon ayarları
    float _tempCalibration1;
    float _tempCalibration2;
    float _humidCalibration1;
    float _humidCalibration2;
    
    // Manuel kuluçka ayarları
    float _manualDevTemp;
    float _manualHatchTemp;
    uint8_t _manualDevHumid;
    uint8_t _manualHatchHumid;
    uint8_t _manualDevDays;
    uint8_t _manualHatchDays;

    struct SystemState {
        float temperature;
        float humidity;
        float targetTemp;
        float targetHumid;
        bool heaterState;
        bool humidifierState;
        bool motorState;
        int pidMode;
        float pidKp;
        float pidKi;
        float pidKd;
        bool alarmEnabled;
        String incubationType;
        bool isIncubationRunning;
    };
    
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
    void _handleRoot();
    void _handleGetStatus();
    void _handleSetParameter();
    void _handleSetTemperature();
    void _handleSetHumidity();
    void _handleSetPidParameters();
    void _handleSetMotorSettings();
    void _handleSetAlarmSettings();
    void _handleSetCalibration();
    void _handleSetIncubationSettings();
    void _handleWiFiConfig();
    void _handleWiFiNetworks();
    void _handleWiFiConnect();
    void _handleMotorTest();         
    void _handleSystemSave();        
    void _handleWiFiCredentials();   
    void _handleSystemHealth();
    
    void _handlePidStatus();         // PID durum endpoint handler'ı
    void _handleMotorStatus();       // Motor durum endpoint handler'ı
    
    // JSON işleme yardımcı fonksiyonları
    void _processParameterUpdate(const String& param, const String& value);
    String _createSuccessResponse();
    String _createErrorResponse(const String& message);
    
    // Bağlantı durumunu kontrol et
    void _checkConnectionStatus();
    
    // WiFi ağlarını tara
    void _scanWiFiNetworks();

    // Memory management
    char* _jsonBuffer;
    char* _responseBuffer;
    bool _buffersAllocated;
    
    // Server lifecycle management
    bool _serverRecreationNeeded;
    unsigned long _lastServerRestart;
    
    // Memory management functions
    bool _allocateBuffers();
    void _deallocateBuffers();
    void _safeServerRestart();

    // Yeniden bağlanma kontrolü için
    unsigned long _lastReconnectAttempt;
    static const unsigned long RECONNECT_INTERVAL = 30000; // 30 saniye

    // Bellek yönetimi fonksiyonu - YENİ EKLENECEK
    void _emergencyMemoryCleanup();
};

#endif // WIFI_MANAGER_H