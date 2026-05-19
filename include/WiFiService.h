#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>

enum class WiFiConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECT_DELAY
};

/**
 * @brief WiFi Connection Manager for Clair System
 * Handles WiFi connection, reconnection, and status monitoring
 */
class WiFiService {
private:
    String ssid;
    String password;

    // Nuevo: Sistema de estados
    WiFiConnectionState state;
    unsigned long stateStartTime;
    unsigned long reconnectDelayStart;
    
    // Configuración
    static const unsigned long CONNECT_TIMEOUT_MS = 10000;     // 10 segundos máximo para conectar
    static const unsigned long RECONNECT_INTERVAL_MS = 30000;  // 30 segundos entre reintentos
    
    // Estadísticas
    int connectionAttempts;
    
    // Callbacks
    void (*onConnectedCallback)();
    void (*onDisconnectedCallback)();
    
    // Método auxiliar
    void changeState(WiFiConnectionState newState);
    
public:
     WiFiService();
    
    void begin(const String& wifiSSID, const String& wifiPassword);
    void update();  // NO bloqueante - llamar en loop()
    void disconnect();
    
    bool isConnected() const { return state == WiFiConnectionState::CONNECTED; }
    String getLocalIP() const { return WiFi.localIP().toString(); }
    String getMacAddress() const { return WiFi.macAddress(); }
    int getRSSI() const { return WiFi.RSSI(); }
    
    void setCallbacks(void (*onConnect)(), void (*onDisconnect)());
    void printStatus();
};
#endif // WIFI_SERVICE_H