#include "CloudService.h"

CloudService::CloudService() 
    : enabled(false), lastSendTime(0), sendInterval(30000),
      successfulSends(0), failedSends(0) {}

void CloudService::begin(const String& url, const String& id, unsigned long interval) {
    endpointUrl = url;
    deviceId = id;
    sendInterval = interval;
    enabled = true;
    // Test connection
    testConnection();
}

String CloudService::buildPayload(const ClairData& data) {
    JsonDocument doc;
    
    // Device info
    doc["deviceId"] = deviceId;
    doc["timestamp"] = data.timestamp;
    
    // Air quality data (SCD41)
    JsonObject airQuality = doc.createNestedObject("airQuality");
    if (data.airQuality.valid) {
        airQuality["co2"] = data.airQuality.co2;
        airQuality["temperature"] = data.airQuality.temperature;
        airQuality["humidity"] = data.airQuality.humidity;
        airQuality["valid"] = true;
    } else {
        airQuality["valid"] = false;
    }
    
    // Particulate matter data (PMS5003)
    JsonObject particulate = doc.createNestedObject("particulateMatter");
    if (data.particulateMatter.valid) {
        particulate["pm1_0"] = data.particulateMatter.pm1_0;
        particulate["pm2_5"] = data.particulateMatter.pm2_5;
        particulate["pm10"] = data.particulateMatter.pm10;
        particulate["valid"] = true;
    } else {
        particulate["valid"] = false;
    }
    
    // AQI
    JsonObject aqi = doc.createNestedObject("aqi");
    if (data.airQualityIndex.aqi >= 0) {
        aqi["value"] = data.airQualityIndex.aqi;
        aqi["category"] = data.airQualityIndex.category;
    }
    
    // Overall status
    doc["status"] = data.statusLabel;
    doc["statusCode"] = data.status;
    
    String payload;
    serializeJson(doc, payload);
    return payload;
}

bool CloudService::sendData(const ClairData& data) {
    if (!enabled) {
        return false;
    }
    
    if (endpointUrl.length() == 0) {
        return false;
    }
    
    String payload = buildPayload(data);
    
    httpClient.begin(endpointUrl);
    httpClient.addHeader(CONTENT_TYPE_HEADER, APPLICATION_JSON);
    
    int httpResponseCode = httpClient.POST(payload);
    httpClient.end();
    
    if (httpResponseCode == 200 || httpResponseCode == 201) {
        successfulSends++;
        return true;
    } else {
        failedSends++;
        return false;
    }
}

bool CloudService::sendDataThrottled(const ClairData& data) {
    if (!enabled) return false;
    
    unsigned long now = millis();
    if (now - lastSendTime >= sendInterval) {
        lastSendTime = now;
        return sendData(data);
    }
    return false;
}

bool CloudService::testConnection() {
    if (endpointUrl.length() == 0) return false;
    
    httpClient.begin(endpointUrl);
    int httpResponseCode = httpClient.GET();
    httpClient.end();
    
    if (httpResponseCode == 200) {
        return true;
    } else {
        return false;
    }
}

void CloudService::printStats() {
}
