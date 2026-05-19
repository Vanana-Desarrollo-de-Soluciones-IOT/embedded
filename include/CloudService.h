#ifndef CLOUD_SERVICE_H
#define CLOUD_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "ClairData.h"

class CloudService {
public:
    CloudService();
    void begin(const String& url, const String& hardwareId, const String& deviceSecret, unsigned long interval);
    bool sendData(const ClairData& data);
    bool sendDataThrottled(const ClairData& data);
    bool testConnection();
    
    // Solo estos métodos adicionales (útiles pero sin romper nada)
    bool isEnabled() const { return enabled; }
    void setEnabled(bool enable) { enabled = enable; }
    
private:
    String buildPayload(const ClairData& data);
    String endpointUrl;
    String hardwareId;
    String deviceSecret;
    bool enabled;
    unsigned long lastSendTime;
    unsigned long sendInterval;
    unsigned long successfulSends;
    unsigned long failedSends;
    HTTPClient httpClient;
};

#endif