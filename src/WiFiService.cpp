#include "WiFiService.h"

WiFiService::WiFiService() 
    : connected(false), lastReconnectAttempt(0), 
      reconnectInterval(30000), connectionAttempts(0),
      onConnectedCallback(nullptr), onDisconnectedCallback(nullptr) {}

void WiFiService::begin(const String& wifiSSID, const String& wifiPassword) {
    ssid = wifiSSID;
    password = wifiPassword;
    
    WiFi.mode(WIFI_STA);
    
    connect();
}

void WiFiService::update() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!connected) {
            connected = true;
            connectionAttempts = 0;
            if (onConnectedCallback) onConnectedCallback();
        }
    } else {
        if (connected) {
            connected = false;
            if (onDisconnectedCallback) onDisconnectedCallback();
        }
        
        // Attempt reconnection
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= reconnectInterval) {
            lastReconnectAttempt = now;
            connect(5000);
        }
    }
}

bool WiFiService::connect(unsigned long timeoutMs) {
    if (ssid.length() == 0) {
        return false;
    }
    
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > timeoutMs) {
            connectionAttempts++;
            return false;
        }
        delay(500);
    }

    connected = true;
    connectionAttempts = 0;
    return true;
}

void WiFiService::disconnect() {
    WiFi.disconnect();
    connected = false;
}

void WiFiService::setCallbacks(void (*onConnect)(), void (*onDisconnect)()) {
    onConnectedCallback = onConnect;
    onDisconnectedCallback = onDisconnect;
}