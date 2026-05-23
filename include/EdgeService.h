#ifndef EDGE_SERVICE_H
#define EDGE_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "ClairData.h"
#include <queue>

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
 * - Cola interna de comandos para procesamiento ordenado
 */
class EdgeService {
private:
    String baseUrl;
    String hardwareId;
    String apiKey;
    
    // Telemetría
    unsigned long lastTelemetryTime;
    unsigned long telemetryInterval;
    bool telemetryEnabled;
    
    // Comandos remotos
    unsigned long lastCommandPollTime;
    unsigned long commandPollInterval;
    CommandCallback commandCallback;
    bool commandsEnabled;
    
    // Cola interna de comandos
    std::queue<RemoteCommand> commandQueue;
    bool processingQueue;
    unsigned long lastCommandProcessTime;
    unsigned long commandProcessDelay;  // Delay entre comandos (ms)
    
    // Estadísticas
    unsigned long telemetrySent;
    unsigned long telemetryFailed;
    unsigned long commandsReceived;
    unsigned long commandsExecuted;
    unsigned long commandsFailed;
    unsigned long commandsQueued;
    
    HTTPClient httpClient;
    
    /**
     * @brief Agrega headers de autenticación
     */
    void addAuthHeaders() {
        httpClient.addHeader("X-Hardware-Id", hardwareId);
        httpClient.addHeader("X-API-Key", apiKey);
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
          commandsReceived(0), commandsExecuted(0), commandsFailed(0), commandsQueued(0),
          processingQueue(false), lastCommandProcessTime(0), commandProcessDelay(500) {}
    
    /**
     * @brief Inicializa el servicio Edge
     * @param url Base URL del Edge (ej: "https://edge.example.com")
     * @param id Hardware ID del dispositivo
     * @param secret API Key
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
        apiKey = secret;
        telemetryInterval = telemetryIntervalMs;
        commandPollInterval = commandPollIntervalMs;
        
        Serial.println("[Edge] Service initialized");
        Serial.print("[Edge] Base URL: ");
        Serial.println(baseUrl.c_str());        
        Serial.print("[Edge] Telemetry interval: ");
        Serial.print(telemetryInterval);
        Serial.println(" ms");
        Serial.print("[Edge] Command poll interval: ");
        Serial.print(commandPollInterval);
        Serial.println(" ms");
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
        Serial.printf("[Edge] Telemetry failed: HTTP %d\r\n", httpCode);
        return false;
    }
    
    /**
     * @brief Consulta comandos pendientes y los agrega a la cola
     * @return true si se encontraron comandos
     */
    bool pollCommands() {
        if (!commandsEnabled) return false;
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
            
            int count = doc["count"] | 0;
            int newCommands = 0;
            
            if (count > 0 && doc["commands"].is<JsonArray>()) {
                JsonArray commands = doc["commands"].as<JsonArray>();
                
                Serial.printf("[Edge] Found %d command(s) in response\r\n", commands.size());
                
                // Agregar TODOS los comandos a la cola
                for (JsonObject cmd : commands) {
                    RemoteCommand remoteCmd;
                    remoteCmd.valid = true;
                    remoteCmd.commandId = cmd["commandId"].as<String>();
                    remoteCmd.type = cmd["type"].as<String>();
                    
                    if (cmd.containsKey("payload") && !cmd["payload"].isNull()) {
                        serializeJson(cmd["payload"], remoteCmd.parameters);
                    }
                    
                    commandQueue.push(remoteCmd);
                    commandsQueued++;
                    newCommands++;
                    commandsReceived++;
                    
                    Serial.printf("[Edge] Queued command: %s (type: %s) - Queue size: %d\r\n", 
                                  remoteCmd.commandId.c_str(), 
                                  remoteCmd.type.c_str(),
                                  commandQueue.size());
                }
                
                Serial.printf("[Edge] Added %d commands to queue\r\n", newCommands);
                return newCommands > 0;
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
                Serial.printf("[Edge] Command poll HTTP error: %d\r\n", httpCode);
            }
        }
        
        return false;
    }
    
    /**
     * @brief Procesa comandos de la cola (llamar periódicamente en loop)
     */
    void processCommandQueue() {
        if (!commandsEnabled) return;
        if (commandCallback == nullptr) return;
        if (commandQueue.empty()) {
            processingQueue = false;
            return;
        }
        
        unsigned long now = millis();
        if (now - lastCommandProcessTime < commandProcessDelay) {
            return;  // Esperar entre comandos
        }
        
        processingQueue = true;
        RemoteCommand cmd = commandQueue.front();
        commandQueue.pop();
        
        Serial.printf("[Edge] Processing queued command: %s (type: %s) - %d remaining\r\n", 
                      cmd.commandId.c_str(), cmd.type.c_str(), commandQueue.size());
        
        bool success = commandCallback(cmd);
        
        if (success) {
            commandsExecuted++;
            sendAck(cmd.commandId, "EXECUTED");
        } else {
            commandsFailed++;
            sendAck(cmd.commandId, "FAILED", "Execution error");
        }
        
        lastCommandProcessTime = now;
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
     * @brief Configura el delay entre procesamiento de comandos
     * @param delayMs Delay en milisegundos
     */
    void setCommandProcessDelay(unsigned long delayMs) { commandProcessDelay = delayMs; }
    
    /**
     * @brief Obtiene el tamaño actual de la cola de comandos
     */
    int getCommandQueueSize() const { return commandQueue.size(); }
    
    /**
     * @brief Limpia la cola de comandos pendientes
     */
    void clearCommandQueue() {
        while (!commandQueue.empty()) {
            commandQueue.pop();
        }
        Serial.println("[Edge] Command queue cleared");
    }
    
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
        Serial.printf("  Telemetry sent:     %lu\r\n", telemetrySent);
        Serial.printf("  Telemetry failed:   %lu\r\n", telemetryFailed);
        Serial.printf("  Commands received:  %lu\r\n", commandsReceived);
        Serial.printf("  Commands queued:    %lu\r\n", commandsQueued);
        Serial.printf("  Commands executed:  %lu\r\n", commandsExecuted);
        Serial.printf("  Commands failed:    %lu\r\n", commandsFailed);
        Serial.printf("  Queue size:         %d\r\n", commandQueue.size());
    }
};

#endif // EDGE_SERVICE_H