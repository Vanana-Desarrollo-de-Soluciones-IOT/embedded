#ifndef INCIDENT_MANAGER_H
#define INCIDENT_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

/**
 * @brief Estructura para un incidente de alerta
 */
struct Incident {
    int id;
    String metric;        // "CO2", "PM25", "TEMP", "HUMIDITY"
    String status;        // "ACTIVE", "RESOLVED"
    String occurredAt;
    String resolvedAt;
    bool acknowledged;    // Si ya se envió ACK
    
    Incident() : id(0), acknowledged(false) {}
    
    void print() const {
        Serial.printf("  Incident #%d: %s [%s]\n", id, metric.c_str(), status.c_str());
        Serial.printf("    Occurred: %s\n", occurredAt.c_str());
        if (resolvedAt.length() > 0) {
            Serial.printf("    Resolved: %s\n", resolvedAt.c_str());
        }
    }
};

/**
 * @brief Estructura para ACK pendiente (payload simplificado)
 */
struct PendingAck {
    int incidentId;
    String status;           // "ACTIVE" o "RESOLVED"
    unsigned long lastAttempt;
    int retryCount;
    bool active;
    
    PendingAck() : incidentId(0), status(""), lastAttempt(0), retryCount(0), active(false) {}
};

/**
 * @brief Clase para manejar incidentes del sistema de alertas
 */
class IncidentManager {
private:
    String baseUrl;
    String hardwareId;
    String apiKey;
    unsigned long pollInterval;
    unsigned long lastPollTime;
    bool enabled;
    
    // Lista de incidentes activos
    Incident activeIncidents[5];
    int incidentCount;
    
    // Queue de ACKs pendientes
    PendingAck pendingAcks[10];  // Máximo 10 ACKs pendientes
    int pendingAckCount;
    static const int MAX_RETRIES = 3;
    static const unsigned long RETRY_INTERVAL_MS = 5000;  // 5 segundos entre reintentos
    
    // Callbacks
    void (*onIncidentDetected)(const Incident&);
    void (*onIncidentResolved)(const Incident&);
    
    HTTPClient httpClient;
    
    void addAuthHeaders() {
        httpClient.addHeader("X-Hardware-Id", hardwareId);
        httpClient.addHeader("X-API-Key", apiKey);
        httpClient.addHeader("Content-Type", "application/json");
    }
    
    int findIncidentById(int id) {
        for (int i = 0; i < incidentCount; i++) {
            if (activeIncidents[i].id == id) {
                return i;
            }
        }
        return -1;
    }
    
    /**
     * @brief Agrega ACK a la cola de pendientes
     */
    void queueAck(int incidentId, const String& status) {
        // Verificar si ya está en la cola para este incidente y estado
        for (int i = 0; i < pendingAckCount; i++) {
            if (pendingAcks[i].active && pendingAcks[i].incidentId == incidentId) {
                Serial.printf("[IncidentManager] ACK for #%d already in queue\n", incidentId);
                return;
            }
        }
        
        if (pendingAckCount >= 10) {
            Serial.println("[IncidentManager] ACK queue full, removing oldest");
            for (int i = 0; i < pendingAckCount - 1; i++) {
                pendingAcks[i] = pendingAcks[i + 1];
            }
            pendingAckCount--;
        }
        
        pendingAcks[pendingAckCount].incidentId = incidentId;
        pendingAcks[pendingAckCount].status = status;  // Guardar el estado
        pendingAcks[pendingAckCount].lastAttempt = 0;
        pendingAcks[pendingAckCount].retryCount = 0;
        pendingAcks[pendingAckCount].active = true;
        pendingAckCount++;
        
        Serial.printf("[IncidentManager] ACK for #%d (status: %s) queued (queue size: %d)\n", 
                      incidentId, status.c_str(), pendingAckCount);
    }
    
    /**
     * @brief Envía ACK con payload simplificado (solo id y status)
     */
    bool sendAckNow(int incidentId) {
        if (!enabled) return false;
        if (WiFi.status() != WL_CONNECTED) return false;
        
        String url = baseUrl + "/api/v1/alerting/incidents/" + String(incidentId) + "/ack";
        
        httpClient.begin(url);
        addAuthHeaders();
        httpClient.setTimeout(3000);
        
        // Payload SIMPLIFICADO: solo id y status
        JsonDocument doc;
        doc["id"] = incidentId;
        doc["status"] = "ACKNOWLEDGED";
        
        String payload;
        serializeJson(doc, payload);
        
        Serial.printf("[IncidentManager] Sending ACK for #%d: %s\n", incidentId, payload.c_str());
        
        int httpCode = httpClient.POST(payload);
        httpClient.end();
        
        if (httpCode == 200 || httpCode == 201 || httpCode == 204) {
            Serial.printf("[IncidentManager] ✅ ACK sent for incident #%d\n", incidentId);
            return true;
        }
        
        Serial.printf("[IncidentManager] ❌ ACK failed for #%d: HTTP %d\n", incidentId, httpCode);
        return false;
    }
    
    /**
     * @brief Procesa la cola de ACKs pendientes
     */
    void processPendingAcks() {
    unsigned long now = millis();
    
    for (int i = 0; i < pendingAckCount; i++) {
        if (!pendingAcks[i].active) continue;
        
        // Verificar si es tiempo de reintentar
        if (now - pendingAcks[i].lastAttempt >= RETRY_INTERVAL_MS) {
            pendingAcks[i].lastAttempt = now;
            pendingAcks[i].retryCount++;
            
            Serial.printf("[IncidentManager] Retrying ACK for #%d (status: %s, attempt %d/%d)\n", 
                          pendingAcks[i].incidentId, 
                          pendingAcks[i].status.c_str(),
                          pendingAcks[i].retryCount, 
                          MAX_RETRIES);
            
            if (sendAckForStatus(pendingAcks[i].incidentId, pendingAcks[i].status)) {
                // ACK exitoso, eliminar de la cola
                pendingAcks[i].active = false;
                
                // Compactar la cola
                for (int j = i; j < pendingAckCount - 1; j++) {
                    pendingAcks[j] = pendingAcks[j + 1];
                }
                pendingAckCount--;
                i--;  // Ajustar índice después de eliminar
            } 
            else if (pendingAcks[i].retryCount >= MAX_RETRIES) {
                // Máximos reintentos alcanzados, abandonar
                Serial.printf("[IncidentManager] ❌ Max retries reached for #%d (status: %s), giving up\n", 
                              pendingAcks[i].incidentId, pendingAcks[i].status.c_str());
                pendingAcks[i].active = false;
                
                // Compactar la cola
                for (int j = i; j < pendingAckCount - 1; j++) {
                    pendingAcks[j] = pendingAcks[j + 1];
                }
                pendingAckCount--;
                i--;
            }
        }
    }
}

    bool sendAckForStatus(int incidentId, const String& status) {
        if (!enabled) return false;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.printf("[IncidentManager] Cannot send ACK for #%d - WiFi not connected\n", incidentId);
            return false;
        }
        
        String url = baseUrl + "/api/v1/alerting/incidents/" + String(incidentId) + "/ack";
        
        httpClient.begin(url);
        addAuthHeaders();
        httpClient.setTimeout(3000);
        
        // Payload simplificado: solo id y status
        JsonDocument doc;
        doc["id"] = incidentId;
        doc["status"] = status;  // "ACKNOWLEDGED" o el estado que corresponda
        
        String payload;
        serializeJson(doc, payload);
        
        Serial.printf("[IncidentManager] Sending ACK for incident #%d with status: %s\n", 
                      incidentId, status.c_str());
        Serial.printf("[IncidentManager] Payload: %s\n", payload.c_str());
        
        int httpCode = httpClient.POST(payload);
        httpClient.end();
        
        if (httpCode == 200 || httpCode == 201 || httpCode == 204) {
            Serial.printf("[IncidentManager] ✅ ACKNOWLEDGED Incident id %d status %s\n", 
                          incidentId, status.c_str());
            return true;
        }
        
        Serial.printf("[IncidentManager] ❌ ACK failed for #%d: HTTP %d\n", incidentId, httpCode);
        return false;
    }
    
    void addIncident(const Incident& incident) {
        // Verificar si ya existe
        if (findIncidentById(incident.id) >= 0) {
            Serial.printf("[IncidentManager] Incident #%d already exists, skipping\n", incident.id);
            return;
        }
        
        if (incidentCount >= 5) {
            Serial.println("[IncidentManager] Too many active incidents, removing oldest");
            for (int i = 0; i < incidentCount - 1; i++) {
                activeIncidents[i] = activeIncidents[i + 1];
            }
            incidentCount--;
        }
        
        activeIncidents[incidentCount] = incident;
        incidentCount++;
        
        Serial.printf("[IncidentManager] ✅ New incident #%d (%s) - Total active: %d\n", 
                      incident.id, incident.metric.c_str(), incidentCount);
        
        // 1. Notificar al dispositivo (activa LED)
        if (onIncidentDetected) {
            onIncidentDetected(incident);
        }
        
        // 2. Encolar ACK para estado ACTIVE
        queueAck(incident.id, "ACTIVE");
    }

    void resolveIncident(int id, const String& resolvedAt) {
        int index = findIncidentById(id);
    if (index >= 0) {
        Incident resolved = activeIncidents[index];
        resolved.status = "RESOLVED";
        resolved.resolvedAt = resolvedAt;
        
        Serial.printf("[IncidentManager] Incident #%d (%s) resolved - Removing from active list\n", 
                      id, resolved.metric.c_str());
        
        // Notificar resolución ANTES de eliminar
        if (onIncidentResolved) {
            onIncidentResolved(resolved);
        }
        
        // Encolar ACK
        queueAck(id, "RESOLVED");
        
        // 🔧 IMPORTANTE: Eliminar del array activo
        for (int i = index; i < incidentCount - 1; i++) {
            activeIncidents[i] = activeIncidents[i + 1];
        }
        incidentCount--;
        
        Serial.printf("[IncidentManager] Active incidents remaining: %d\n", incidentCount);
    } else {
        Serial.printf("[IncidentManager] WARNING: Attempted to resolve incident #%d but not found in active list\n", id);
    }
}
    
public:
    IncidentManager() 
        : enabled(false), lastPollTime(0), pollInterval(5000),
          incidentCount(0), pendingAckCount(0),
          onIncidentDetected(nullptr), onIncidentResolved(nullptr) {}
    
    /**
     * @brief Inicializa el manager de incidentes
     */
    void begin(const String& url, const String& id, const String& key, unsigned long interval = 5000) {
        baseUrl = url;
        if (baseUrl.endsWith("/")) {
            baseUrl = baseUrl.substring(0, baseUrl.length() - 1);
        }
        hardwareId = id;
        apiKey = key;
        pollInterval = interval;
        enabled = true;
        
        Serial.println("[IncidentManager] Initialized");
        Serial.printf("  Endpoint: %s\n", baseUrl.c_str());
        Serial.printf("  Poll interval: %lu ms\n", pollInterval);
    }
    
    /**
     * @brief Consulta incidentes pendientes
     */
    // En IncidentManager.h, modificar pollIncidents() para manejar múltiples incidentes de la misma métrica

bool pollIncidents() {
    if (!enabled) return false;
    if (WiFi.status() != WL_CONNECTED) return false;
    
    unsigned long now = millis();
    if (now - lastPollTime < pollInterval) {
        return false;
    }
    lastPollTime = now;
    
    String url = baseUrl + "/api/v1/alerting/incidents/pending";
    
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
            Serial.printf("[IncidentManager] JSON parse error: %s\n", error.c_str());
            return false;
        }
        
        int count = doc["count"] | 0;
        
        if (count > 0 && doc["events"].is<JsonArray>()) {
            JsonArray events = doc["events"].as<JsonArray>();
            
            Serial.printf("[IncidentManager] Processing %d event(s)\n", events.size());
            
            // 🔧 NUEVO: Marcar qué métricas tienen incidentes ACTIVOS en este poll
            bool metricHasActive[4] = {false, false, false, false}; // CO2, PM25, TEMP, HUMIDITY
            
            for (JsonObject evt : events) {
                int id = evt["id"] | 0;
                String metric = evt["metric"] | "";
                String status = evt["status"] | "";
                String occurredAt = evt["occurred_at"] | "";
                String resolvedAt = evt["resolved_at"] | "";
                
                // Registrar qué métricas tienen ACTIVE
                if (status == "ACTIVE") {
                    if (metric == "CO2") metricHasActive[0] = true;
                    else if (metric == "PM25") metricHasActive[1] = true;
                    else if (metric == "TEMP") metricHasActive[2] = true;
                    else if (metric == "HUMIDITY") metricHasActive[3] = true;
                }
                
                int existingIndex = findIncidentById(id);
                
                if (status == "ACTIVE") {
                    if (existingIndex < 0) {
                        // 🔧 NUEVO: Verificar si ya hay un incidente activo de la misma métrica
                        int sameMetricIndex = -1;
                        for (int i = 0; i < incidentCount; i++) {
                            if (activeIncidents[i].metric == metric) {
                                sameMetricIndex = i;
                                break;
                            }
                        }
                        
                        if (sameMetricIndex >= 0) {
                            // Reemplazar el incidente antiguo por el nuevo
                            Serial.printf("[IncidentManager] ⚠️ Replacing old %s incident #%d with new #%d\n", 
                                          metric.c_str(), activeIncidents[sameMetricIndex].id, id);
                            
                            // Notificar resolución del antiguo
                            if (onIncidentResolved) {
                                onIncidentResolved(activeIncidents[sameMetricIndex]);
                            }
                            
                            // Reemplazar
                            activeIncidents[sameMetricIndex].id = id;
                            activeIncidents[sameMetricIndex].metric = metric;
                            activeIncidents[sameMetricIndex].status = status;
                            activeIncidents[sameMetricIndex].occurredAt = occurredAt;
                            activeIncidents[sameMetricIndex].resolvedAt = resolvedAt;
                            activeIncidents[sameMetricIndex].acknowledged = false;
                            
                            // Notificar nuevo incidente
                            if (onIncidentDetected) {
                                onIncidentDetected(activeIncidents[sameMetricIndex]);
                            }
                            
                            queueAck(id, "ACTIVE");
                        } else {
                            // Incidente nuevo de métrica sin incidente activo
                            Incident incident;
                            incident.id = id;
                            incident.metric = metric;
                            incident.status = status;
                            incident.occurredAt = occurredAt;
                            incident.resolvedAt = resolvedAt;
                            incident.acknowledged = false;
                            addIncident(incident);
                        }
                    }
                } 
                else if (status == "RESOLVED") {
                    if (existingIndex >= 0) {
                        resolveIncident(id, resolvedAt);
                    } else {
                        // 🔧 NUEVO: Buscar incidente de la misma métrica y resolverlo
                        int sameMetricIndex = -1;
                        for (int i = 0; i < incidentCount; i++) {
                            if (activeIncidents[i].metric == metric) {
                                sameMetricIndex = i;
                                break;
                            }
                        }
                        
                        if (sameMetricIndex >= 0) {
                            Serial.printf("[IncidentManager] 🔧 Resolving %s incident #%d (old) via resolution of #%d\n", 
                                          metric.c_str(), activeIncidents[sameMetricIndex].id, id);
                            
                            // Resolver el incidente antiguo
                            resolveIncident(activeIncidents[sameMetricIndex].id, resolvedAt);
                        } else {
                            Serial.printf("[IncidentManager] Incident #%d resolved but not in active list\n", id);
                            
                            if (onIncidentResolved) {
                                Incident dummyIncident;
                                dummyIncident.id = id;
                                dummyIncident.metric = metric;
                                dummyIncident.status = "RESOLVED";
                                dummyIncident.resolvedAt = resolvedAt;
                                onIncidentResolved(dummyIncident);
                            }
                            
                            queueAck(id, "RESOLVED");
                        }
                    }
                }
            }
            
            // 🔧 NUEVO: Limpiar incidentes de métricas que ya no tienen ACTIVE
            for (int i = incidentCount - 1; i >= 0; i--) {
                bool shouldBeActive = false;
                if (activeIncidents[i].metric == "CO2") shouldBeActive = metricHasActive[0];
                else if (activeIncidents[i].metric == "PM25") shouldBeActive = metricHasActive[1];
                else if (activeIncidents[i].metric == "TEMP") shouldBeActive = metricHasActive[2];
                else if (activeIncidents[i].metric == "HUMIDITY") shouldBeActive = metricHasActive[3];
                
                if (!shouldBeActive && activeIncidents[i].status == "ACTIVE") {
                    Serial.printf("[IncidentManager] 🧹 Cleaning stale %s incident #%d (no longer active on server)\n", 
                                  activeIncidents[i].metric.c_str(), activeIncidents[i].id);
                    
                    if (onIncidentResolved) {
                        onIncidentResolved(activeIncidents[i]);
                    }
                    
                    // Eliminar del array
                    for (int j = i; j < incidentCount - 1; j++) {
                        activeIncidents[j] = activeIncidents[j + 1];
                    }
                    incidentCount--;
                }
            }
            
            return incidentCount > 0;
        }
    }
    else if (httpCode == 404 || httpCode == 204) {
        httpClient.end();
        
        if (incidentCount > 0) {
            Serial.printf("[IncidentManager] No pending incidents - Clearing all %d active incidents\n", incidentCount);
            
            for (int i = 0; i < incidentCount; i++) {
                if (onIncidentResolved) {
                    onIncidentResolved(activeIncidents[i]);
                }
                queueAck(activeIncidents[i].id, "RESOLVED");
            }
            
            incidentCount = 0;
        }
        return false;
    }
    else {
        httpClient.end();
        if (httpCode > 0) {
            Serial.printf("[IncidentManager] HTTP error: %d\n", httpCode);
        }
    }
    
    return false;
}
    
    /**
     * @brief Procesa ACKs pendientes (llamar en update)
     */
    void process() {
        processPendingAcks();
    }
    
    /**
     * @brief Fuerza el envío de ACK para un incidente específico
     */
    bool forceAck(int incidentId) {
        return sendAckNow(incidentId);
    }
    
    /**
     * @brief Obtiene prioridad de métrica
     */
    int getMetricPriority(const String& metric) const {
        if (metric == "CO2") return 1;
        if (metric == "PM25") return 2;
        if (metric == "TEMP") return 3;
        if (metric == "HUMIDITY") return 4;
        return 5;
    }
    
    /**
     * @brief Obtiene el incidente más crítico
     */
    Incident getMostCriticalIncident() const {
        if (incidentCount == 0) return Incident();
        
        int bestIndex = 0;
        int bestPriority = getMetricPriority(activeIncidents[0].metric);
        
        for (int i = 1; i < incidentCount; i++) {
            int priority = getMetricPriority(activeIncidents[i].metric);
            if (priority < bestPriority) {
                bestPriority = priority;
                bestIndex = i;
            }
        }
        
        return activeIncidents[bestIndex];
    }
    
    /**
     * @brief Verifica si hay incidentes activos
     */
    bool hasActiveIncidents() const { return incidentCount > 0; }
    
    /**
     * @brief Obtiene el conteo de incidentes activos
     */
    int getActiveCount() const { return incidentCount; }
    
    /**
     * @brief Obtiene el conteo de ACKs pendientes
     */
    int getPendingAckCount() const { return pendingAckCount; }
    
    /**
     * @brief Configura callbacks
     */
    void setCallbacks(void (*onDetect)(const Incident&), void (*onResolve)(const Incident&)) {
        onIncidentDetected = onDetect;
        onIncidentResolved = onResolve;
    }
    
    /**
     * @brief Imprime estadísticas
     */
    void printStats() {
        Serial.println("\n[IncidentManager] Statistics:");
        Serial.printf("  Active incidents: %d\n", incidentCount);
        Serial.printf("  Pending ACKs: %d\n", pendingAckCount);
        
        if (incidentCount > 0) {
            Serial.println("  Active incidents list:");
            for (int i = 0; i < incidentCount; i++) {
                activeIncidents[i].print();
            }
        }
        
        if (pendingAckCount > 0) {
            Serial.println("  Pending ACKs:");
            for (int i = 0; i < pendingAckCount; i++) {
                if (pendingAcks[i].active) {
                    Serial.printf("    #%d (retries: %d)\n", 
                                  pendingAcks[i].incidentId, 
                                  pendingAcks[i].retryCount);
                }
            }
        }
    }
    
    bool isEnabled() const { return enabled; }
    void setEnabled(bool enable) { enabled = enable; }
};

#endif // INCIDENT_MANAGER_H