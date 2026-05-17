#include "WiFiService.h"

WiFiService::WiFiService() 
    : connected(false), lastReconnectAttempt(0), 
      reconnectInterval(30000), connectionAttempts(0),
      onConnectedCallback(nullptr), onDisconnectedCallback(nullptr) {}

void WiFiService::begin(const String& wifiSSID, const String& wifiPassword) {
    ssid = wifiSSID;
    password = wifiPassword;
    
    Serial.println("\n📡 Initializing WiFi...");
    WiFi.mode(WIFI_STA);
    
    connect();
}

void WiFiService::update() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!connected) {
            connected = true;
            connectionAttempts = 0;
            Serial.println("✅ WiFi connected!");
            if (onConnectedCallback) onConnectedCallback();
        }
    } else {
        if (connected) {
            connected = false;
            Serial.println("❌ WiFi disconnected!");
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
        Serial.println("⚠️ No WiFi credentials configured");
        return false;
    }
    
    Serial.printf("📡 Connecting to WiFi: %s...\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > timeoutMs) {
            Serial.println("❌ WiFi connection timeout!");
            connectionAttempts++;
            return false;
        }
        delay(500);
        Serial.print(".");
    }
    
    Serial.println();
    connected = true;
    connectionAttempts = 0;
    return true;
}

void WiFiService::disconnect() {
    WiFi.disconnect();
    connected = false;
    Serial.println("🔌 WiFi disconnected");
}

void WiFiService::setCallbacks(void (*onConnect)(), void (*onDisconnect)()) {
    onConnectedCallback = onConnect;
    onDisconnectedCallback = onDisconnect;
}

void WiFiService::printStatus() {
    Serial.println("\n📡 WiFi Status:");
    Serial.println("═══════════════════════════════");
    Serial.printf("  SSID:        %s\n", ssid.c_str());
    Serial.printf("  Status:      %s\n", isConnected() ? "Connected" : "Disconnected");
    if (isConnected()) {
        Serial.printf("  IP Address:  %s\n", getLocalIP().c_str());
        Serial.printf("  MAC Address: %s\n", getMacAddress().c_str());
        Serial.printf("  RSSI:        %d dBm\n", getRSSI());
    }
    Serial.println("═══════════════════════════════\n");
}