#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "config.h"
#include "storage.h"
#include "watchdog_manager.h"

enum OTAState {
    OTA_IDLE,
    OTA_UPLOADING,
    OTA_SUCCESS,
    OTA_ERROR,
    OTA_VALIDATING
};

class OTAManager {
public:
    OTAManager();
    
    bool begin();
    
    void setStorage(Storage* storage) { _storage = storage; }
    void setWatchdog(WatchdogManager* watchdog) { _watchdog = watchdog; }
    
    bool startUpdate(size_t contentLength, String md5 = "");
    bool writeChunk(uint8_t* data, size_t length);
    bool endUpdate();
    void abortUpdate();
    
    OTAState getState() const { return _state; }
    int getProgress() const;
    String getErrorMessage() const { return _errorMessage; }
    
    bool isUpdateInProgress() const { return _state == OTA_UPLOADING; }
    
    void checkRollback();
    bool validateFirmware();
    
    size_t getTotalSize() const { return _totalSize; }
    size_t getWrittenSize() const { return _writtenSize; }
    
    String getFirmwareVersion() const { return FIRMWARE_VERSION; }
    String getBuildDate() const { return __DATE__ " " __TIME__; }
    
private:
    OTAState _state;
    size_t _totalSize;
    size_t _writtenSize;
    String _errorMessage;
    String _expectedMD5;
    
    Storage* _storage;
    WatchdogManager* _watchdog;
    
    unsigned long _updateStartTime;
    unsigned long _lastProgressReport;
    
    bool _saveSystemState();
    bool _restoreSystemState();
    void _clearUpdateFlags();
    
    static const size_t MIN_FIRMWARE_SIZE = 100000;  // 100KB minimum
    static const size_t MAX_FIRMWARE_SIZE = 1900000; // 1.9MB maximum
    static const unsigned long UPDATE_TIMEOUT = 300000; // 5 dakika
};

#endif