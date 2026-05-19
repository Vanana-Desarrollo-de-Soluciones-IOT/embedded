#ifndef CLOUD_SERVICE_H
#define CLOUD_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "ClairData.h"

/**
 * @brief Cloud Service for sending data to remote endpoints
 * Handles HTTP POST requests with sensor data
 */
class CloudService {
public:
    CloudService();
    void begin(const String& url, const String& hardwareId, const String& deviceSecret, unsigned long interval);
    bool sendData(const ClairData& data);
    bool sendDataThrottled(const ClairData& data);
    bool testConnection();
    void printStats();
    bool isEnabled() const { return enabled; }  // Añadir 'const'
    void setEnabled(bool enable) { enabled = enable; }  // Añadir este método
    bool isConnected() { return enabled && endpointUrl.length() > 0; }  // Opcional: método útil
    
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
    WiFiClient client;
    HTTPClient httpClient;
};
#endif // CLOUD_SERVICE_H