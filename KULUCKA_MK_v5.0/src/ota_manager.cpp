#include "ota_manager.h"
#include <esp_ota_ops.h>

// FIRMWARE_VERSION tanımlaması kaldırıldı - config.h'tan gelecek

OTAManager::OTAManager() {
    _state = OTA_IDLE;
    _totalSize = 0;
    _writtenSize = 0;
    _storage = nullptr;
    _watchdog = nullptr;
    _updateStartTime = 0;
    _lastProgressReport = 0;
}

bool OTAManager::begin() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* update = esp_ota_get_next_update_partition(NULL);
    
    if (!running || !update) {
        Serial.println("OTA: Partition bilgileri alınamadı!");
        return false;
    }
    
    Serial.printf("OTA: Running partition: %s [0x%08x] %d bytes\n", 
                  running->label, running->address, running->size);
    Serial.printf("OTA: Update partition: %s [0x%08x] %d bytes\n", 
                  update->label, update->address, update->size);
    
    checkRollback();
    
    return true;
}

bool OTAManager::startUpdate(size_t contentLength, String md5) {
    if (_state == OTA_UPLOADING) {
        _errorMessage = "Güncelleme zaten devam ediyor";
        return false;
    }
    
    if (contentLength < MIN_FIRMWARE_SIZE || contentLength > MAX_FIRMWARE_SIZE) {
        _errorMessage = "Geçersiz firmware boyutu: " + String(contentLength);
        _state = OTA_ERROR;
        return false;
    }
    
    Serial.println("OTA: Güncelleme başlatılıyor - Boyut: " + String(contentLength) + " bytes");
    
    if (_storage) {
        _storage->saveStateNow();
        delay(100);
    }
    
    if (!_saveSystemState()) {
        Serial.println("OTA: Sistem durumu kaydedilemedi!");
    }
    
    if (_watchdog) {
        _watchdog->beginOperation(OP_CUSTOM, "OTA Güncelleme");
    }
    
    if (!Update.begin(contentLength)) {
        _errorMessage = String("Başlatma hatası: ") + Update.errorString();
        _state = OTA_ERROR;
        Serial.println("OTA: " + _errorMessage);
        return false;
    }
    
    if (md5.length() > 0) {
        Update.setMD5(md5.c_str());
        _expectedMD5 = md5;
        Serial.println("OTA: MD5 doğrulaması aktif");
    }
    
    _totalSize = contentLength;
    _writtenSize = 0;
    _state = OTA_UPLOADING;
    _updateStartTime = millis();
    _lastProgressReport = millis();
    
    Serial.println("OTA: Güncelleme başarıyla başlatıldı");
    return true;
}

bool OTAManager::writeChunk(uint8_t* data, size_t length) {
    if (_state != OTA_UPLOADING) {
        _errorMessage = "Güncelleme başlatılmamış";
        return false;
    }
    
    if (millis() - _updateStartTime > UPDATE_TIMEOUT) {
        _errorMessage = "Güncelleme zaman aşımı";
        abortUpdate();
        return false;
    }
    
    size_t written = Update.write(data, length);
    if (written != length) {
        _errorMessage = String("Yazma hatası: ") + Update.errorString();
        _state = OTA_ERROR;
        abortUpdate();
        return false;
    }
    
    _writtenSize += written;
    
    if (_watchdog && millis() - _lastProgressReport > 1000) {
        _watchdog->feed();
        _lastProgressReport = millis();
        
        int progress = getProgress();
        Serial.printf("OTA: İlerleme %d%% (%d/%d bytes)\n", 
                      progress, _writtenSize, _totalSize);
    }
    
    return true;
}

bool OTAManager::endUpdate() {
    if (_state != OTA_UPLOADING) {
        _errorMessage = "Güncelleme devam etmiyor";
        return false;
    }
    
    _state = OTA_VALIDATING;
    Serial.println("OTA: Güncelleme tamamlanıyor ve doğrulanıyor...");
    
    if (!Update.end(true)) {
        _errorMessage = String("Tamamlama hatası: ") + Update.errorString();
        _state = OTA_ERROR;
        Serial.println("OTA: " + _errorMessage);
        return false;
    }
    
    _state = OTA_SUCCESS;
    Serial.println("OTA: Güncelleme başarıyla tamamlandı!");
    
    if (_watchdog) {
        _watchdog->endOperation();
    }
    
    _clearUpdateFlags();
    
    delay(1000);
    Serial.println("OTA: Sistem yeniden başlatılıyor...");
    ESP.restart();
    
    return true;
}

void OTAManager::abortUpdate() {
    if (_state == OTA_UPLOADING) {
        Update.abort();
        _state = OTA_ERROR;
        
        if (_watchdog) {
            _watchdog->endOperation();
        }
        
        _restoreSystemState();
        Serial.println("OTA: Güncelleme iptal edildi");
    }
}

int OTAManager::getProgress() const {
    if (_totalSize == 0) return 0;
    return (_writtenSize * 100) / _totalSize;
}

void OTAManager::checkRollback() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            Serial.println("OTA: Firmware doğrulaması bekleniyor...");
            
            if (validateFirmware()) {
                esp_ota_mark_app_valid_cancel_rollback();
                Serial.println("OTA: Firmware doğrulandı ve kalıcı olarak işaretlendi");
            } else {
                Serial.println("OTA: Firmware doğrulaması başarısız, rollback yapılacak");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
    }
}

bool OTAManager::validateFirmware() {
    if (!_storage) return true;
    
    if (_storage->getIncubationType() > 3) return false;
    if (_storage->getTargetTemperature() < 20 || _storage->getTargetTemperature() > 45) return false;
    if (_storage->getTargetHumidity() < 30 || _storage->getTargetHumidity() > 90) return false;
    
    Serial.println("OTA: Firmware doğrulama başarılı");
    return true;
}

bool OTAManager::_saveSystemState() {
    if (!_storage) return false;
    
    _storage->saveStateNow();
    return true;
}

bool OTAManager::_restoreSystemState() {
    if (!_storage) return false;
    
    return true;
}

void OTAManager::_clearUpdateFlags() {
    // Güncelleme bayraklarını temizle
}