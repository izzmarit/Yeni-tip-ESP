/**
 * @file incubation.cpp
 * @brief Kuluçka tipleri ve kontrolü uygulaması
 * @version 1.1
 */

#include "incubation.h"

Incubation::Incubation() {
    _activeType = INCUBATION_CHICKEN; // Varsayılan olarak tavuk kuluçkası
    _isRunning = false;
    _currentStage = STAGE_DEVELOPMENT;
    
    // Kuluçka parametrelerini başlat
    _initializeParameters();
}

bool Incubation::begin() {
    // Kuluçka kontrolü başlatma
    return true;
}

void Incubation::_initializeParameters() {
    // Tavuk kuluçka parametreleri
    _chickenParams.developmentTemp = 37.8;
    _chickenParams.hatchingTemp = 37.5;
    _chickenParams.developmentHumidity = 60;
    _chickenParams.hatchingHumidity = 70;
    _chickenParams.developmentDays = 18;
    _chickenParams.hatchingDays = 3;
    _chickenParams.totalDays = 21;
    _chickenParams.name = "Tavuk";
    
    // Bıldırcın kuluçka parametreleri
    _quailParams.developmentTemp = 37.5;
    _quailParams.hatchingTemp = 36.5;
    _quailParams.developmentHumidity = 60;
    _quailParams.hatchingHumidity = 70;
    _quailParams.developmentDays = 15;
    _quailParams.hatchingDays = 3;
    _quailParams.totalDays = 18;
    _quailParams.name = "Bildircin";
    
    // Kaz kuluçka parametreleri
    _gooseParams.developmentTemp = 37.4;
    _gooseParams.hatchingTemp = 36.9;
    _gooseParams.developmentHumidity = 55;
    _gooseParams.hatchingHumidity = 75;
    _gooseParams.developmentDays = 28;
    _gooseParams.hatchingDays = 3;
    _gooseParams.totalDays = 31;
    _gooseParams.name = "Kaz";
    
    // Manuel kuluçka parametreleri (varsayılan değerler)
    _manualParams.developmentTemp = 37.5;
    _manualParams.hatchingTemp = 37.0;
    _manualParams.developmentHumidity = 60;
    _manualParams.hatchingHumidity = 70;
    _manualParams.developmentDays = 18;
    _manualParams.hatchingDays = 3;
    _manualParams.totalDays = 21;
    _manualParams.name = "Manuel";
}

void Incubation::setIncubationType(uint8_t type) {
    if (type <= INCUBATION_MANUAL) {
        _activeType = type;
    }
}

uint8_t Incubation::getIncubationType() const {
    return _activeType;
}

String Incubation::getIncubationTypeName() const {
    return _getCurrentParameters().name;
}

bool Incubation::startIncubation(DateTime startTime) {
    _isRunning = true;
    _startTime = startTime;
    _currentStage = STAGE_DEVELOPMENT;
    return true;
}

void Incubation::stopIncubation() {
    _isRunning = false;
}

bool Incubation::isIncubationRunning() const {
    return _isRunning;
}

IncubationParameters Incubation::getParameters() const {
    return _getCurrentParameters();
}

void Incubation::setManualParameters(float devTemp, float hatchTemp, uint8_t devHumid, 
                         uint8_t hatchHumid, uint8_t devDays, uint8_t hatchDays) {
    _manualParams.developmentTemp = devTemp;
    _manualParams.hatchingTemp = hatchTemp;
    _manualParams.developmentHumidity = devHumid;
    _manualParams.hatchingHumidity = hatchHumid;
    _manualParams.developmentDays = devDays;
    _manualParams.hatchingDays = hatchDays;
    _manualParams.totalDays = devDays + hatchDays;
}

void Incubation::setTargetTemperature(float temperature) {
    // Mevcut kuluçka aşamasına göre doğru parametreyi ayarla
    if (_currentStage == STAGE_DEVELOPMENT) {
        if (_activeType == INCUBATION_MANUAL) {
            _manualParams.developmentTemp = temperature;
        }
        // Diğer kuluçka tipleri için standart değerler kullanılıyor
        // İstenirse burada diğer kuluçka tipleri için de ayarlamalar yapılabilir
    } else if (_currentStage == STAGE_HATCHING) {
        if (_activeType == INCUBATION_MANUAL) {
            _manualParams.hatchingTemp = temperature;
        }
        // Diğer kuluçka tipleri için standart değerler kullanılıyor
    }
}

void Incubation::setTargetHumidity(uint8_t humidity) {
    // Mevcut kuluçka aşamasına göre doğru parametreyi ayarla
    if (_currentStage == STAGE_DEVELOPMENT) {
        if (_activeType == INCUBATION_MANUAL) {
            _manualParams.developmentHumidity = humidity;
        }
        // Diğer kuluçka tipleri için standart değerler kullanılıyor
    } else if (_currentStage == STAGE_HATCHING) {
        if (_activeType == INCUBATION_MANUAL) {
            _manualParams.hatchingHumidity = humidity;
        }
        // Diğer kuluçka tipleri için standart değerler kullanılıyor
    }
}

IncubationStage Incubation::getCurrentStage() const {
    return _currentStage;
}

uint8_t Incubation::getCurrentDay(DateTime currentTime) const {
    if (!_isRunning) {
        return 0;
    }
    
    // Başlangıç ve mevcut zaman arasındaki farkı hesapla
    TimeSpan diff = currentTime - _startTime;
    uint8_t daysPassed = diff.days() + 1; // İlk gün 1 olarak sayılır
    
    // Eğer kuluçka süresi aşıldıysa son günü döndür
    if (daysPassed > _getCurrentParameters().totalDays) {
        return _getCurrentParameters().totalDays;
    }
    
    return daysPassed;
}

DateTime Incubation::getStartTime() const {
    return _startTime;
}

float Incubation::getTargetTemperature() const {
    IncubationParameters params = _getCurrentParameters();
    
    if (_currentStage == STAGE_DEVELOPMENT) {
        return params.developmentTemp;
    } else {
        return params.hatchingTemp;
    }
}

uint8_t Incubation::getTargetHumidity() const {
    IncubationParameters params = _getCurrentParameters();
    
    if (_currentStage == STAGE_DEVELOPMENT) {
        return params.developmentHumidity;
    } else {
        return params.hatchingHumidity;
    }
}

uint8_t Incubation::getTotalDays() const {
    return _getCurrentParameters().totalDays;
}

void Incubation::update(DateTime currentTime) {
    if (!_isRunning) {
        return;
    }
    
    // Mevcut kuluçka gününü hesapla
    uint8_t currentDay = getCurrentDay(currentTime);
    IncubationParameters params = _getCurrentParameters();
    
    // Kuluçka aşamasını kontrol et
    if (currentDay > params.developmentDays) {
        _currentStage = STAGE_HATCHING;
    } else {
        _currentStage = STAGE_DEVELOPMENT;
    }
    
    // Kuluçka süresinin bitip bitmediğini kontrol et
    if (currentDay > params.totalDays) {
        stopIncubation();
    }
}

void Incubation::saveState() {
    // Bu metot, Storage sınıfı tarafından çağrılacak
    // ve kuluçka durumunun EEPROM/Flash'a kaydedilmesini sağlayacak
}

void Incubation::loadState() {
    // Bu metot, Storage sınıfı tarafından çağrılacak
    // ve kuluçka durumunun EEPROM/Flash'dan yüklenmesini sağlayacak
}

IncubationParameters Incubation::_getCurrentParameters() const {
    return _getParametersForType(_activeType);
}

IncubationParameters Incubation::_getParametersForType(uint8_t type) const {
    switch (type) {
        case INCUBATION_CHICKEN:
            return _chickenParams;
        case INCUBATION_QUAIL:
            return _quailParams;
        case INCUBATION_GOOSE:
            return _gooseParams;
        case INCUBATION_MANUAL:
            return _manualParams;
        default:
            return _chickenParams; // Varsayılan olarak tavuk parametrelerini döndür
    }
}