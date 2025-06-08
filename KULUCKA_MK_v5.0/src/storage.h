/**
 * @file storage.h
 * @brief Ayarlar ve durumları saklama modülü
 * @version 1.2
 */

#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>
#include <RTClib.h>
#include "config.h"

// WiFi çalışma modları
enum WiFiConnectionMode {
    WIFI_CONN_MODE_AP,      // Access Point modu
    WIFI_CONN_MODE_STATION  // Station modu (evdeki ağa bağlan)
};

// Saklama veri yapısı
struct StorageData {
    // Kuluçka ayarları
    uint8_t incubationType;           // Kuluçka tipi
    float manualDevTemp;              // Manuel gelişim sıcaklığı
    float manualHatchTemp;            // Manuel çıkım sıcaklığı
    uint8_t manualDevHumid;           // Manuel gelişim nemi
    uint8_t manualHatchHumid;         // Manuel çıkım nemi
    uint8_t manualDevDays;            // Manuel gelişim günleri
    uint8_t manualHatchDays;          // Manuel çıkım günleri
    
    // Çalışma durumu
    bool isIncubationRunning;         // Kuluçka çalışıyor mu?
    uint32_t startTimeUnix;           // Kuluçka başlangıç zamanı (Unix timestamp)
    
    // PID ayarları
    float pidKp;                      // PID Kp değeri
    float pidKi;                      // PID Ki değeri
    float pidKd;                      // PID Kd değeri
    
    // Motor ayarları
    uint32_t motorWaitTime;           // Motor bekleme süresi (dakika)
    uint32_t motorRunTime;            // Motor çalışma süresi (saniye)
    
    // Kalibrasyon ayarları
    float tempCalibration1;           // 1. sıcaklık sensörü kalibrasyonu
    float tempCalibration2;           // 2. sıcaklık sensörü kalibrasyonu
    float humidCalibration1;          // 1. nem sensörü kalibrasyonu
    float humidCalibration2;          // 2. nem sensörü kalibrasyonu
    
    // Alarm ayarları
    float tempLowAlarm;               // Düşük sıcaklık alarm eşiği
    float tempHighAlarm;              // Yüksek sıcaklık alarm eşiği
    float humidLowAlarm;              // Düşük nem alarm eşiği
    float humidHighAlarm;             // Yüksek nem alarm eşiği
    bool alarmsEnabled;               // Tüm alarmlar etkin mi?
    
    // WiFi ayarları
    char wifiSSID[32];                // WiFi SSID
    char wifiPassword[32];            // WiFi Şifresi
    bool wifiEnabled;                 // WiFi etkin mi?
    WiFiConnectionMode wifiMode;      // WiFi bağlantı modu (AP/Station)
    char stationSSID[32];             // Station modunda bağlanılacak ağ SSID'si
    char stationPassword[32];         // Station modunda bağlanılacak ağ şifresi
    
    // Veri geçerlilik işareti
    uint32_t validationCode;          // 0xABCD1234 değeri saklayarak verilerin geçerli olduğunu doğrulayacak
};

class Storage {
public:
    // Yapılandırıcı
    Storage();
    
    // Saklama modülünü başlat
    bool begin();
    
    // Durum ve parametre değişikliklerini anında kaydet
    void saveStateNow();
    
    // Değişiklikleri kaydet - gecikmeli kayıt yapar
    bool queueSave();
    
    // Kayıt kuyruğunu işle (belirli bir süre geçtiyse veya çok sayıda değişiklik varsa)
    void processQueue();
    
    // Son kaydedilen zamandan bu yana geçen süre
    unsigned long getTimeSinceLastSave() const;
    
    // Varsayılan ayarları yükle
    void loadDefaults();
    
    // Veriyi alma ve ayarlama metotları
    uint8_t getIncubationType() const;
    void setIncubationType(uint8_t type);
    
    bool isIncubationRunning() const;
    void setIncubationRunning(bool running);
    
    DateTime getStartTime() const;
    void setStartTime(DateTime startTime);
    
    float getManualDevTemp() const;
    void setManualDevTemp(float temp);
    
    float getManualHatchTemp() const;
    void setManualHatchTemp(float temp);
    
    uint8_t getManualDevHumid() const;
    void setManualDevHumid(uint8_t humid);
    
    uint8_t getManualHatchHumid() const;
    void setManualHatchHumid(uint8_t humid);
    
    uint8_t getManualDevDays() const;
    void setManualDevDays(uint8_t days);
    
    uint8_t getManualHatchDays() const;
    void setManualHatchDays(uint8_t days);
    
    float getPidKp() const;
    void setPidKp(float kp);
    
    float getPidKi() const;
    void setPidKi(float ki);
    
    float getPidKd() const;
    void setPidKd(float kd);
    
    uint32_t getMotorWaitTime() const;
    void setMotorWaitTime(uint32_t minutes);
    
    uint32_t getMotorRunTime() const;
    void setMotorRunTime(uint32_t seconds);
    
    float getTempCalibration(uint8_t sensorIndex) const;
    void setTempCalibration(uint8_t sensorIndex, float value);
    
    float getHumidCalibration(uint8_t sensorIndex) const;
    void setHumidCalibration(uint8_t sensorIndex, float value);
    
    float getTempLowAlarm() const;
    void setTempLowAlarm(float value);
    
    float getTempHighAlarm() const;
    void setTempHighAlarm(float value);
    
    float getHumidLowAlarm() const;
    void setHumidLowAlarm(float value);
    
    float getHumidHighAlarm() const;
    void setHumidHighAlarm(float value);
    
    // Yeni: Alarm durumu
    bool areAlarmsEnabled() const;
    void setAlarmsEnabled(bool enabled);
    
    String getWifiSSID() const;
    void setWifiSSID(const String& ssid);
    
    String getWifiPassword() const;
    void setWifiPassword(const String& password);
    
    bool isWifiEnabled() const;
    void setWifiEnabled(bool enabled);
    
    // Yeni WiFi ayarları
    WiFiConnectionMode getWifiMode() const;
    void setWifiMode(WiFiConnectionMode mode);
    
    String getStationSSID() const;
    void setStationSSID(const String& ssid);
    
    String getStationPassword() const;
    void setStationPassword(const String& password);
    
    // Veri yapısına tüm ayarları kaydet
    void getData(StorageData& data) const;
    
    // Veri yapısından tüm ayarları ayarla
    void setData(const StorageData& data);

private:
    StorageData _data;
    bool _isInitialized;
    
    // Kayıt kuyruğu değişkenleri
    unsigned long _lastSaveTime;
    uint8_t _pendingChanges;
    bool _saveScheduled;
    
    // Doğrulama kodu
    static const uint32_t VALIDATION_CODE = 0xABCD1234;
    
    // EEPROM'a veri yaz
    template<typename T>
    void _writeToEEPROM(int address, const T& value);
    
    // EEPROM'dan veri oku
    template<typename T>
    void _readFromEEPROM(int address, T& value);
    
    // Ayarları EEPROM'a yazma işlemi
    bool saveSettings();
    
    // Ayarları EEPROM'dan yükleme işlemi
    bool loadSettings();
};

#endif // STORAGE_H