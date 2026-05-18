#include "CloudService.h"
#include <WiFi.h>

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
    doc["uptime"] = millis() / 1000; 
    
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

    

    //Connectivity info
    JsonObject connectivity = doc.createNestedObject("connectivity");

    if (WiFi.status() == WL_CONNECTED) {
        connectivity["status"] = "connected";
        connectivity["ssid"] = WiFi.SSID();
        connectivity["ip"] = WiFi.localIP().toString();
        connectivity["rssi"] = WiFi.RSSI();
        connectivity["mac"] = WiFi.macAddress();
        connectivity["channel"] = WiFi.channel();
    } else {
        connectivity["status"] = "disconnected";
        connectivity["ssid"] = "none";
        connectivity["ip"] = "0.0.0.0";
        connectivity["rssi"] = 0;
        connectivity["mac"] = WiFi.macAddress();
        connectivity["channel"] = 0;
    }
    
    // Device health
    JsonObject health = doc.createNestedObject("deviceHealth");
    
    // Free heap and memory stats
    health["freeHeap"] = ESP.getFreeHeap();
    health["minFreeHeap"] = ESP.getMinFreeHeap();
    health["heapSize"] = ESP.getHeapSize();
    health["maxAllocHeap"] = ESP.getMaxAllocHeap();
    
    // Sensor status
    health["scd41Status"] = data.airQuality.valid ? "ok" : "error";
    health["pms5003Status"] = data.particulateMatter.valid ? "ok" : "error";
    
    // Time since last valid sensor reading
    static unsigned long lastValidAirQualityTime = 0;
    static unsigned long lastValidPMTime = 0;
    
    if (data.airQuality.valid) {
        lastValidAirQualityTime = millis();
    }
    if (data.particulateMatter.valid) {
        lastValidPMTime = millis();
    }
    
    health["lastValidAirQualitySec"] = (millis() - lastValidAirQualityTime) / 1000;
    health["lastValidPMSec"] = (millis() - lastValidPMTime) / 1000;

    //Device Info
    JsonObject deviceInfo = doc.createNestedObject("deviceInfo");
    deviceInfo["chipModel"] = ESP.getChipModel();
    deviceInfo["chipRevision"] = ESP.getChipRevision();
    deviceInfo["cpuFreqMHz"] = ESP.getCpuFreqMHz();
    deviceInfo["flashSize"] = ESP.getFlashChipSize();
    deviceInfo["sketchSize"] = ESP.getSketchSize();
    deviceInfo["freeSketchSpace"] = ESP.getFreeSketchSpace();
    
    //Overall status
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
        Serial.println("[CloudService] Data sent successfully");  
        return true;
    } else {
        failedSends++;
        Serial.print("[CloudService] Failed to send data. HTTP code: ");
        Serial.println(httpResponseCode); 
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
