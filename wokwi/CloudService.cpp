#include "CloudService.h"

CloudService::CloudService() 
    : enabled(false), lastSendTime(0), sendInterval(30000),
      successfulSends(0), failedSends(0) {}

void CloudService::begin(const String& url, const String& id, unsigned long interval) {
    endpointUrl = url;
    deviceId = id;
    sendInterval = interval;
    enabled = true;
    
    Serial.println("\n☁️ Initializing Cloud Service...");
    Serial.printf("  → Endpoint: %s\n", endpointUrl.c_str());
    Serial.printf("  → Device ID: %s\n", deviceId.c_str());
    Serial.printf("  → Send Interval: %lu ms\n", sendInterval);
    
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
        Serial.println("☁️ Cloud service disabled");
        return false;
    }
    
    if (endpointUrl.length() == 0) {
        Serial.println("❌ No endpoint URL configured");
        return false;
    }
    
    String payload = buildPayload(data);
    
    Serial.println("\n☁️ Sending data to cloud...");
    Serial.printf("  → Payload size: %d bytes\n", payload.length());
    
    httpClient.begin(endpointUrl);
    httpClient.addHeader(CONTENT_TYPE_HEADER, APPLICATION_JSON);
    
    int httpResponseCode = httpClient.POST(payload);
    String response = httpClient.getString();
    
    httpClient.end();
    
    if (httpResponseCode == 200 || httpResponseCode == 201) {
        Serial.printf("  ✅ Success! HTTP %d\n", httpResponseCode);
        successfulSends++;
        
        // Parse response if needed
        JsonDocument responseDoc;
        deserializeJson(responseDoc, response);
        
        return true;
    } else {
        Serial.printf("  ❌ Failed! HTTP %d\n", httpResponseCode);
        Serial.printf("  Response: %s\n", response.c_str());
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
    
    Serial.println("☁️ Testing cloud connection...");
    
    httpClient.begin(endpointUrl);
    int httpResponseCode = httpClient.GET();
    httpClient.end();
    
    if (httpResponseCode == 200) {
        Serial.println("  ✅ Connection successful");
        return true;
    } else {
        Serial.printf("  ⚠️ Connection test returned HTTP %d\n", httpResponseCode);
        return false;
    }
}

void CloudService::printStats() {
    Serial.println("\n☁️ Cloud Service Statistics:");
    Serial.println("═══════════════════════════════");
    Serial.printf("  Status:        %s\n", enabled ? "Enabled" : "Disabled");
    Serial.printf("  Successful:    %lu\n", successfulSends);
    Serial.printf("  Failed:        %lu\n", failedSends);
    Serial.printf("  Success Rate:  %.1f%%\n", 
                  (successfulSends + failedSends) > 0 ? 
                  (float)successfulSends / (successfulSends + failedSends) * 100 : 0);
    Serial.println("═══════════════════════════════\n");
}