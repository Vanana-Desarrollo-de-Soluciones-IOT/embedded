// WiFiService.cpp - Versión corregida (sin delay bloqueante)

#include "WiFiService.h"

WiFiService::WiFiService() 
    : state(WiFiConnectionState::DISCONNECTED),
      stateStartTime(0),
      reconnectDelayStart(0),
      connectionAttempts(0),
      onConnectedCallback(nullptr),
      onDisconnectedCallback(nullptr) {}

void WiFiService::begin(const String& wifiSSID, const String& wifiPassword) {
    ssid = wifiSSID;
    password = wifiPassword;
    
    WiFi.mode(WIFI_STA);
    
    // Iniciar conexión sin bloquear
    changeState(WiFiConnectionState::CONNECTING);
    WiFi.begin(ssid.c_str(), password.c_str());
    stateStartTime = millis();
}

void WiFiService::changeState(WiFiConnectionState newState) {
    state = newState;
    stateStartTime = millis();
}

void WiFiService::update() {
    unsigned long now = millis();
    
    switch (state) {
        case WiFiConnectionState::CONNECTING:
            // Verificar si ya conectó
            if (WiFi.status() == WL_CONNECTED) {
                changeState(WiFiConnectionState::CONNECTED);
                connectionAttempts = 0;
                if (onConnectedCallback) onConnectedCallback();
                Serial.println("[WiFi] Connected successfully");
            }
            // Verificar timeout
            else if (now - stateStartTime >= CONNECT_TIMEOUT_MS) {
                Serial.println("[WiFi] Connection timeout");
                WiFi.disconnect();  // Limpiar estado
                changeState(WiFiConnectionState::RECONNECT_DELAY);
                reconnectDelayStart = now;
                connectionAttempts++;
            }
            break;
            
        case WiFiConnectionState::CONNECTED:
            // Monitorear si se perdió la conexión
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[WiFi] Connection lost");
                changeState(WiFiConnectionState::RECONNECT_DELAY);
                reconnectDelayStart = now;
                if (onDisconnectedCallback) onDisconnectedCallback();
            }
            break;
            
        case WiFiConnectionState::RECONNECT_DELAY:
            // Esperar antes de reintentar
            if (now - reconnectDelayStart >= RECONNECT_INTERVAL_MS) {
                Serial.println("[WiFi] Attempting reconnection...");
                changeState(WiFiConnectionState::CONNECTING);
                WiFi.begin(ssid.c_str(), password.c_str());
            }
            break;
            
        case WiFiConnectionState::DISCONNECTED:
        default:
            // No hacer nada, esperar comando externo
            break;
    }
}

void WiFiService::disconnect() {
    WiFi.disconnect();
    changeState(WiFiConnectionState::DISCONNECTED);
    if (onDisconnectedCallback) onDisconnectedCallback();
}

void WiFiService::setCallbacks(void (*onConnect)(), void (*onDisconnect)()) {
    onConnectedCallback = onConnect;
    onDisconnectedCallback = onDisconnect;
}

void WiFiService::printStatus() {
    Serial.print("[WiFi] State: ");
    switch (state) {
        case WiFiConnectionState::DISCONNECTED:
            Serial.print("DISCONNECTED");
            break;
        case WiFiConnectionState::CONNECTING:
            Serial.print("CONNECTING");
            break;
        case WiFiConnectionState::CONNECTED:
            Serial.print("CONNECTED");
            break;
        case WiFiConnectionState::RECONNECT_DELAY:
            Serial.print("RECONNECT_DELAY");
            break;
    }
    
    if (state == WiFiConnectionState::CONNECTED) {
        Serial.print(" | IP: ");
        Serial.print(getLocalIP());
        Serial.print(" | RSSI: ");
        Serial.print(getRSSI());
        Serial.print(" dBm");
    }
    Serial.println();
}