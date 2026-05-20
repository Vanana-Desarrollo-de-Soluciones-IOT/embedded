#ifndef REMOTE_COMMAND_CLIENT_H
#define REMOTE_COMMAND_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/**
 * @brief Estructura para un comando recibido del Edge
 */
struct RemoteCommand {
    String commandId;      // ID único del comando
    String type;           // STANDBY, WAKE, RESTART, REPORT, etc.
    String parameters;     // Parámetros adicionales en formato JSON
    bool valid;            // Si el comando es válido
    
    RemoteCommand() : valid(false) {}
};

/**
 * @brief Cliente para consultar y ejecutar comandos remotos desde el Edge
 */
class RemoteCommandClient {
private:
    String edgeEndpoint;
    String hardwareId;
    String deviceSecret;
    unsigned long pollInterval;      // Intervalo entre consultas
    unsigned long lastPollTime;      // Última consulta realizada
    
    HTTPClient httpClient;
    bool enabled;
    
    // Estadísticas
    unsigned long commandsReceived;
    unsigned long commandsExecuted;
    unsigned long commandsFailed;
    
    /**
     * @brief Construye los headers para autenticación
     */
    void addAuthHeaders() {
        httpClient.addHeader("X-Hardware-Id", hardwareId);
        httpClient.addHeader("X-Device-Secret", deviceSecret);
        httpClient.addHeader("Content-Type", "application/json");
    }
    
    /**
     * @brief Envía ACK al Edge
     * @param commandId ID del comando
     * @param status "EXECUTED" o "FAILED"
     * @param failureReason Razón del fallo (opcional)
     * @return true si se envió exitosamente
     */
    bool sendAck(const String& commandId, const String& status, const String& failureReason = "") {
        if (!enabled) return false;
        if (WiFi.status() != WL_CONNECTED) return false;
        
        String url = edgeEndpoint + "/api/v1/device/commands/" + commandId + "/ack";
        
        httpClient.begin(url);
        addAuthHeaders();
        httpClient.setTimeout(5000);
        
        // Construir payload
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
            Serial.printf("[RemoteCmd] ACK sent for %s: %s\n", commandId.c_str(), status.c_str());
            return true;
        }
        
        Serial.printf("[RemoteCmd] Failed to send ACK for %s: HTTP %d\n", commandId.c_str(), httpCode);
        return false;
    }
    
public:
    RemoteCommandClient() 
        : enabled(false), lastPollTime(0), pollInterval(10000),
          commandsReceived(0), commandsExecuted(0), commandsFailed(0) {}
    
    /**
     * @brief Inicializa el cliente de comandos remotos
     * @param endpoint URL del Edge (ej: "https://edge.example.com")
     * @param id Hardware ID del dispositivo
     * @param secret Device Secret
     * @param interval Intervalo de polling en ms (default: 10000)
     */
    void begin(const String& endpoint, const String& id, const String& secret, unsigned long interval = 10000) {
        edgeEndpoint = endpoint;
        // Eliminar trailing slash si existe
        if (edgeEndpoint.endsWith("/")) {
            edgeEndpoint = edgeEndpoint.substring(0, edgeEndpoint.length() - 1);
        }
        hardwareId = id;
        deviceSecret = secret;
        pollInterval = interval;
        enabled = true;
        
        Serial.println("[RemoteCmd] Client initialized");
        Serial.printf("[RemoteCmd] Endpoint: %s\n", edgeEndpoint.c_str());
        Serial.printf("[RemoteCmd] Poll interval: %lu ms\n", pollInterval);
    }
    
    /**
     * @brief Consulta comandos pendientes (debe llamarse periódicamente)
     * @param callback Función que procesará el comando recibido
     * @return true si se recibió y procesó un comando
     */
    bool pollCommands(bool (*callback)(const RemoteCommand&)) {
        if (!enabled) return false;
        if (WiFi.status() != WL_CONNECTED) return false;
        
        unsigned long now = millis();
        if (now - lastPollTime < pollInterval) {
            return false;
        }
        lastPollTime = now;
        
        String url = edgeEndpoint + "/api/v1/device/commands/pending";
        
        httpClient.begin(url);
        addAuthHeaders();
        httpClient.setTimeout(5000);
        
        int httpCode = httpClient.GET();
        
        if (httpCode == 200) {
            String payload = httpClient.getString();
            httpClient.end();
            
            // Parsear respuesta
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (error) {
                Serial.println("[RemoteCmd] Failed to parse response");
                return false;
            }
            
            // Verificar si hay comando
            if (doc.containsKey("command")) {
                JsonObject cmd = doc["command"];
                
                RemoteCommand remoteCmd;
                remoteCmd.valid = true;
                remoteCmd.commandId = cmd["id"].as<String>();
                remoteCmd.type = cmd["type"].as<String>();
                
                if (cmd.containsKey("parameters")) {
                    serializeJson(cmd["parameters"], remoteCmd.parameters);
                }
                
                commandsReceived++;
                Serial.printf("[RemoteCmd] Received command: %s (type: %s)\n", 
                              remoteCmd.commandId.c_str(), remoteCmd.type.c_str());
                
                // Ejecutar comando a través del callback
                bool success = callback(remoteCmd);
                
                // Enviar ACK
                if (success) {
                    commandsExecuted++;
                    sendAck(remoteCmd.commandId, "EXECUTED");
                } else {
                    commandsFailed++;
                    sendAck(remoteCmd.commandId, "FAILED", "Execution error");
                }
                
                return true;
            }
        } 
        else if (httpCode == 204 || httpCode == 404) {
            // No hay comandos pendientes - respuesta normal
            httpClient.end();
            return false;
        }
        else {
            httpClient.end();
            if (httpCode > 0) {
                Serial.printf("[RemoteCmd] HTTP error: %d\n", httpCode);
            }
        }
        
        return false;
    }
    
    /**
     * @brief Verifica si el cliente está habilitado
     */
    bool isEnabled() const { return enabled; }
    
    /**
     * @brief Habilita/deshabilita el cliente
     */
    void setEnabled(bool enable) { enabled = enable; }
    
    /**
     * @brief Obtiene estadísticas
     */
    void printStats() {
        Serial.println("\n[RemoteCmd] Statistics:");
        Serial.printf("  Commands received: %lu\n", commandsReceived);
        Serial.printf("  Commands executed: %lu\n", commandsExecuted);
        Serial.printf("  Commands failed:   %lu\n", commandsFailed);
    }
};

#endif // REMOTE_COMMAND_CLIENT_H