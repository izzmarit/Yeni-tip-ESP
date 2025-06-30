/**
 * @file hysteresis.h
 * @brief Histerezis (On/Off) kontrol modülü
 * @version 1.0
 */

#ifndef HYSTERESIS_H
#define HYSTERESIS_H

#include <Arduino.h>
#include "config.h"

class Hysteresis {
public:
    // Yapılandırıcı
    Hysteresis();
    
    // Histerezis kontrolünü başlat
    bool begin();
    
    // Hedef değeri ayarla
    void setSetpoint(double setpoint);
    
    // Düşük eşik değerini ayarla (yüzde olarak)
    void setLowThreshold(double threshold);
    
    // Yüksek eşik değerini ayarla (yüzde olarak)
    void setHighThreshold(double threshold);
    
    // Mevcut değeri hesapla ve çıkışı güncelle
    void compute(double input);
    
    // Çıkış durumunu al
    bool getOutput() const;
    
    // Hedef değeri al
    double getSetpoint() const;
    
    // Düşük eşik değerini al
    double getLowThreshold() const;
    
    // Yüksek eşik değerini al
    double getHighThreshold() const;
    
    // Hedeften sapma değerini al
    double getDeviation() const;

private:
    // Kontrol parametreleri
    double _setpoint;     // Hedef değer
    double _lowThreshold; // Düşük eşik değeri (yüzde olarak)
    double _highThreshold; // Yüksek eşik değeri (yüzde olarak)
    
    // Kontrol değişkenleri
    double _input;        // Giriş değeri
    bool _output;         // Çıkış durumu (aktif/pasif)
    
    // Hesaplanan düşük ve yüksek sınırlar
    double _lowLimit;     // Alt sınır
    double _highLimit;    // Üst sınır
    
    // Son hesaplanan sapma
    double _lastDeviation;
    
    // Sınırları hesapla
    void _calculateLimits();
};

#endif // HYSTERESIS_H