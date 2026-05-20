#ifndef EDGE_SERVICE_H
#define EDGE_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "ClairData.h"

/**
 * @brief Estructura para un comando recibido del Edge
 */
struct RemoteCommand {
    String commandId;
    String type;
    String parameters;
    bool valid;
    
    RemoteCommand() : valid(false) {}
};

/**
 * @brief Callback para procesar comandos remotos
 * @return true si el comando se ejecutó correctamente
 */
typedef bool (*CommandCallback)(const RemoteCommand&);

/**
 * @brief Servicio unificado para comunicación con el Edge
 * 
 * Maneja:
 * - Envío de telemetría (CloudService)
 * - Consulta de comandos remotos (RemoteCommandClient)
 * - Envío de ACKs
 */
class EdgeService {
private:
    String baseUrl;
    String hardwareId;
    String deviceSecret;
    
    // Telemetría
    unsigned long lastTelemetryTime;
    unsigned long telemetryInterval;
    bool telemetryEnabled;
    
    // Comandos remotos
    unsigned long lastCommandPollTime;
    unsigned long commandPollInterval;
    CommandCallback commandCallback;
    bool commandsEnabled;
    
    // Estadísticas
    unsigned long telemetrySent;
    unsigned long telemetryFailed;
    unsigned long commandsReceived;
    unsigned long commandsExecuted;
    unsigned long commandsFailed;
    
    HTTPClient httpClient;
    
    /**
     * @brief Agrega headers de autenticación
     */
    void addAuthHeaders() {
        httpClient.addHeader("X-Hardware-Id", hardwareId);
        httpClient.addHeader("X-Device-Secret", deviceSecret);
        httpClient.addHeader("Content-Type", "application/json");
    }
    
    /**
     * @brief Construye payload de telemetría
     */
    String buildTelemetryPayload(const ClairData& data) {
        JsonDocument doc;
        
        doc["deviceId"] = hardwareId;
        
        if (data.timeFormatted.length() > 0) {
            doc["timestamp"] = data.timeFormatted;
        } else {
            doc["timestamp"] = data.timestamp;
        }
        
        if (data.uptimeFormatted.length() > 0) {
            doc["uptime"] = data.uptimeFormatted;
        } else {
            doc["uptime"] = millis() / 1000;
        }
        
        // Air quality
        JsonObject airQuality = doc.createNestedObject("airQuality");
        if (data.airQuality.valid) {
            airQuality["co2"] = data.airQuality.co2;
            airQuality["temperature"] = data.airQuality.temperature;
            airQuality["humidity"] = data.airQuality.humidity;
        }
        
        // Particulate matter
        JsonObject particulate = doc.createNestedObject("particulateMatter");
        if (data.particulateMatter.valid) {
            particulate["pm1_0"] = data.particulateMatter.pm1_0;
            particulate["pm2_5"] = data.particulateMatter.pm2_5;
            particulate["pm10"] = data.particulateMatter.pm10;
        }
        
        // Connectivity
        JsonObject connectivity = doc.createNestedObject("connectivity");
        if (WiFi.status() == WL_CONNECTED) {
            connectivity["status"] = "connected";
            connectivity["network"] = WiFi.SSID();
            connectivity["signalStrength"] = WiFi.RSSI();
        } else {
            connectivity["status"] = "disconnected";
            connectivity["network"] = "none";
            connectivity["signalStrength"] = 0;
        }
        
        // Location & status
        JsonObject location = doc.createNestedObject("location");
        location["country"] = data.country;
        
        doc["healthStatus"] = calculateHealthStatus(data);
        doc["status"] = data.statusLabel;
        
        String payload;
        serializeJson(doc, payload);
        return payload;
    }
    
    /**
     * @brief Calcula health status simplificado
     */
    int calculateHealthStatus(const ClairData& data) {
        int health = 100;
        health -= (data.airQuality.valid ? 0 : 25);
        health -= (data.particulateMatter.valid ? 0 : 25);
        health -= (WiFi.status() == WL_CONNECTED ? (WiFi.RSSI() < -70 ? 15 : 0) : 30);
        return (health < 0) ? 0 : (health > 100 ? 100 : health);
    }
    
    /**
     * @brief Envía ACK al Edge
     */
    bool sendAck(const String& commandId, const String& status, const String& failureReason = "") {
        if (!commandsEnabled) return false;
        if (WiFi.status() != WL_CONNECTED) return false;
        
        String url = baseUrl + "/api/v1/device/commands/" + commandId + "/ack";
        
        httpClient.begin(url);
        addAuthHeaders();
        httpClient.setTimeout(5000);
        
        JsonDocument doc;
        doc["status"] = status;
        if (failureReason.length() > 0) {
            doc["failureReason"] = failureReason;
        }
        
        String payload;
        serializeJson(doc, payload);
        
        int httpCode = httpClient.POST(payload);
        httpClient.end();
        
        if (httpCode == 200 || httpCode == 201 || httpCode == 204) {
            Serial.printf("[Edge] ACK sent for %s: %s\n", commandId.c_str(), status.c_str());
            return true;
        }
        
        Serial.printf("[Edge] Failed to send ACK for %s: HTTP %d\n", commandId.c_str(), httpCode);
        return false;
    }
    
public:
    EdgeService() 
        : telemetryInterval(15000), telemetryEnabled(true),
          commandPollInterval(10000), commandCallback(nullptr), commandsEnabled(true),
          telemetrySent(0), telemetryFailed(0),
          commandsReceived(0), commandsExecuted(0), commandsFailed(0) {}
    
    /**
     * @brief Inicializa el servicio Edge
     * @param url Base URL del Edge (ej: "https://edge.example.com")
     * @param id Hardware ID del dispositivo
     * @param secret Device Secret
     * @param telemetryIntervalMs Intervalo de envío de telemetría (ms)
     * @param commandPollIntervalMs Intervalo de consulta de comandos (ms)
     */
    void begin(const String& url, const String& id, const String& secret, 
               unsigned long telemetryIntervalMs = 15000,
               unsigned long commandPollIntervalMs = 10000) {
        baseUrl = url;
        // Eliminar trailing slash
        if (baseUrl.endsWith("/")) {
            baseUrl = baseUrl.substring(0, baseUrl.length() - 1);
        }
        hardwareId = id;
        deviceSecret = secret;
        telemetryInterval = telemetryIntervalMs;
        commandPollInterval = commandPollIntervalMs;
        
        Serial.println("[Edge] Service initialized");
        Serial.printf("[Edge] Base URL: %s\n", baseUrl.c_str());
        Serial.printf("[Edge] Telemetry interval: %lu ms\n", telemetryInterval);
        Serial.printf("[Edge] Command poll interval: %lu ms\n", commandPollInterval);
    }
    
    /**
     * @brief Envía telemetría (con throttling)
     * @param data Datos del dispositivo
     * @return true si se envió (respetando intervalo)
     */
    bool sendTelemetry(const ClairData& data) {
        if (!telemetryEnabled) return false;
        if (WiFi.status() != WL_CONNECTED) return false;
        
        unsigned long now = millis();
        if (now - lastTelemetryTime < telemetryInterval) {
            return false;
        }
        lastTelemetryTime = now;
        
        String url = baseUrl + "/api/v1/device/telemetry";
        String payload = buildTelemetryPayload(data);
        
        httpClient.begin(url);
        addAuthHeaders();
        httpClient.setTimeout(5000);
        
        int httpCode = httpClient.POST(payload);
        httpClient.end();
        
        if (httpCode == 200 || httpCode == 201) {
            telemetrySent++;
            Serial.println("[Edge] Telemetry sent successfully");
            return true;
        }
        
        telemetryFailed++;
        Serial.printf("[Edge] Telemetry failed: HTTP %d\n", httpCode);
        return false;
    }
    
    /**
     * @brief Consulta comandos pendientes (llamar periódicamente)
     * @return true si se procesó un comando
     */
    bool pollCommands() {
    if (!commandsEnabled) return false;
    if (commandCallback == nullptr) return false;
    if (WiFi.status() != WL_CONNECTED) return false;
    
    unsigned long now = millis();
    if (now - lastCommandPollTime < commandPollInterval) {
        return false;
    }
    lastCommandPollTime = now;
    
    String url = baseUrl + "/api/v1/device/commands/pending";
    
    httpClient.begin(url);
    addAuthHeaders();
    httpClient.setTimeout(5000);
    
    int httpCode = httpClient.GET();
    
    if (httpCode == 200) {
        String payload = httpClient.getString();
        httpClient.end();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            Serial.printf("[Edge] Failed to parse command response: %s\n", error.c_str());
            return false;
        }
        
        // Verificar la nueva estructura: { "count": X, "commands": [...] }
        int count = doc["count"] | 0;
        
        if (count > 0 && doc["commands"].is<JsonArray>()) {
            JsonArray commands = doc["commands"].as<JsonArray>();
            
            for (JsonObject cmd : commands) {
                RemoteCommand remoteCmd;
                remoteCmd.valid = true;
                remoteCmd.commandId = cmd["commandId"].as<String>();  // Nota: es "commandId", no "id"
                remoteCmd.type = cmd["type"].as<String>();
                
                // El payload puede estar en "payload"
                if (cmd.containsKey("payload") && !cmd["payload"].isNull()) {
                    serializeJson(cmd["payload"], remoteCmd.parameters);
                }
                
                commandsReceived++;
                Serial.printf("[Edge] Received command: %s (type: %s)\n", 
                              remoteCmd.commandId.c_str(), remoteCmd.type.c_str());
                
                // Ejecutar comando a través del callback
                bool success = commandCallback(remoteCmd);
                
                if (success) {
                    commandsExecuted++;
                    sendAck(remoteCmd.commandId, "EXECUTED");
                } else {
                    commandsFailed++;
                    sendAck(remoteCmd.commandId, "FAILED", "Execution error");
                }
                
                return true;  // Procesar un comando por ciclo
            }
        } else {
            Serial.println("[Edge] No pending commands");
        }
    } 
    else if (httpCode == 204 || httpCode == 404) {
        // No hay comandos - respuesta normal
        httpClient.end();
        return false;
    }
    else {
        httpClient.end();
        if (httpCode > 0) {
            Serial.printf("[Edge] Command poll HTTP error: %d\n", httpCode);
        }
    }
    
    return false;
}
    
    /**
     * @brief Establece el callback para comandos remotos
     */
    void setCommandCallback(CommandCallback callback) {
        commandCallback = callback;
    }
    
    /**
     * @brief Habilita/deshabilita envío de telemetría
     */
    void setTelemetryEnabled(bool enabled) { telemetryEnabled = enabled; }
    bool isTelemetryEnabled() const { return telemetryEnabled; }
    
    /**
     * @brief Habilita/deshabilita consulta de comandos
     */
    void setCommandsEnabled(bool enabled) { commandsEnabled = enabled; }
    bool isCommandsEnabled() const { return commandsEnabled; }
    
    /**
     * @brief Fuerza envío inmediato de telemetría
     */
    bool forceTelemetry(const ClairData& data) {
        lastTelemetryTime = 0;  // Reset para evitar throttling
        return sendTelemetry(data);
    }
    
    /**
     * @brief Imprime estadísticas
     */
    void printStats() {
        Serial.println("\n[Edge] Statistics:");
        Serial.printf("  Telemetry sent:   %lu\n", telemetrySent);
        Serial.printf("  Telemetry failed: %lu\n", telemetryFailed);
        Serial.printf("  Commands received: %lu\n", commandsReceived);
        Serial.printf("  Commands executed: %lu\n", commandsExecuted);
        Serial.printf("  Commands failed:   %lu\n", commandsFailed);
    }
};

#endif // EDGE_SERVICE_H