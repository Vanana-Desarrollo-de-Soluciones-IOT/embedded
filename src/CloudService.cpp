#include "CloudService.h"
#include <WiFi.h>

CloudService::CloudService() 
    : enabled(false), lastSendTime(0), sendInterval(30000),
      successfulSends(0), failedSends(0) {}

void CloudService::begin(const String& url, const String& id, const String& secret, unsigned long interval) {
    endpointUrl = url;
    hardwareId = id;
    deviceSecret = secret;
    sendInterval = interval;
    enabled = true;
    testConnection();
}

String CloudService::buildPayload(const ClairData& data) {
    JsonDocument doc;
    
    // Device info
    doc["deviceId"] = hardwareId;
    // NUEVO: timestamp en formato HH:MM:SS
    if (data.timeFormatted.length() > 0) {
        doc["timestamp"] = data.timeFormatted;  // String "14:30:25"
    } else {
        doc["timestamp"] = data.timestamp;  // Fallback a millis desde boot
    }
    
    // NUEVO: uptime en formato HH:MM:SS
    if (data.uptimeFormatted.length() > 0) {
        doc["uptime"] = data.uptimeFormatted;
    } else {
        doc["uptime"] = millis() / 1000;
    }

    
    // Air quality data (SCD41)
    JsonObject airQuality = doc.createNestedObject("airQuality");
    if (data.airQuality.valid) {
        airQuality["co2"] = data.airQuality.co2;
        airQuality["temperature"] = data.airQuality.temperature;
        airQuality["humidity"] = data.airQuality.humidity;        
    } 

    // Particulate matter data (PMS5003)
    JsonObject particulate = doc.createNestedObject("particulateMatter");
    if (data.particulateMatter.valid) {
        particulate["pm1_0"] = data.particulateMatter.pm1_0;
        particulate["pm2_5"] = data.particulateMatter.pm2_5;
        particulate["pm10"] = data.particulateMatter.pm10;
    } 

    // Connectivity info
    JsonObject connectivity = doc.createNestedObject("connectivity");
    if (WiFi.status() == WL_CONNECTED) {
        connectivity["status"] = "connected";        
    } else {
        connectivity["status"] = "disconnected";       
    }         
    
    // Overall status
    doc["status"] = data.statusLabel;
 
    
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
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[CloudService] WiFi not connected");
        return false;
    }
    
    String payload = buildPayload(data);
    
    // Opcional: descomentar para debug del payload
    // Serial.print("[CloudService] Payload: ");
    // Serial.println(payload);
    
    httpClient.begin(endpointUrl);
    httpClient.addHeader("Content-Type", "application/json");
    httpClient.addHeader("X-Hardware-Id", hardwareId);
    httpClient.addHeader("X-Device-Secret", deviceSecret);
    httpClient.setTimeout(5000);  // Timeout de 5 segundos
    
    int httpResponseCode = httpClient.POST(payload);
    httpClient.end();
    
    if (httpResponseCode == 200 || httpResponseCode == 201) {
        successfulSends++;
        Serial.println("[CloudService] Data sent successfully");
        return true;
    }
    
    failedSends++;
    
    // Logging más informativo
    if (httpResponseCode > 0) {
        Serial.printf("[CloudService] HTTP error: %d\n", httpResponseCode);
    } else if (httpResponseCode == -1) {
        Serial.println("[CloudService] Connection timeout");
    } else {
        Serial.printf("[CloudService] Connection error: %d\n", httpResponseCode);
    }
    
    return false;
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
    
    Serial.println("[CloudService] Cloud service configured with URL: " + endpointUrl);
    return true;
}