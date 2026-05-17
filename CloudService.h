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
private:
    String endpointUrl;
    String deviceId;
    HTTPClient httpClient;
    bool enabled;
    unsigned long lastSendTime;
    unsigned long sendInterval;
    
    // Statistics
    unsigned long successfulSends;
    unsigned long failedSends;
    
    // Headers
    static constexpr const char* CONTENT_TYPE_HEADER = "Content-Type";
    static constexpr const char* APPLICATION_JSON = "application/json";
    
    /**
     * @brief Build JSON payload from ClairData
     */
    String buildPayload(const ClairData& data);
    
public:
    CloudService();
    
    /**
     * @brief Initialize cloud service
     * @param url Endpoint URL
     * @param id Device ID
     * @param interval Send interval in milliseconds
     */
    void begin(const String& url, const String& id, unsigned long interval = 30000);
    
    /**
     * @brief Send data to cloud
     * @param data ClairData to send
     * @return true if successful
     */
    bool sendData(const ClairData& data);
    
    /**
     * @brief Send data with automatic rate limiting
     * @param data ClairData to send
     * @return true if sent (respects interval)
     */
    bool sendDataThrottled(const ClairData& data);
    
    /**
     * @brief Enable/disable cloud sending
     */
    void setEnabled(bool enable) { enabled = enable; }
    
    /**
     * @brief Check if cloud service is enabled
     */
    bool isEnabled() const { return enabled; }
    
    /**
     * @brief Set send interval
     */
    void setSendInterval(unsigned long interval) { sendInterval = interval; }
    
    /**
     * @brief Get statistics
     */
    void printStats();
    
    /**
     * @brief Test connection to endpoint
     */
    bool testConnection();
};

#endif // CLOUD_SERVICE_H