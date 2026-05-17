#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>

/**
 * @brief WiFi Connection Manager for Clair System
 * Handles WiFi connection, reconnection, and status monitoring
 */
class WiFiService {
private:
    String ssid;
    String password;
    bool connected;
    unsigned long lastReconnectAttempt;
    unsigned long reconnectInterval;
    int connectionAttempts;
    
    // Callback for connection status changes
    void (*onConnectedCallback)();
    void (*onDisconnectedCallback)();
    
public:
    WiFiService();
    
    /**
     * @brief Initialize WiFi with credentials
     * @param wifiSSID WiFi SSID
     * @param wifiPassword WiFi password
     */
    void begin(const String& wifiSSID, const String& wifiPassword);
    
    /**
     * @brief Update WiFi connection status (call in loop)
     */
    void update();
    
    /**
     * @brief Connect to WiFi (blocking)
     * @param timeoutMs Timeout in milliseconds
     * @return true if connected
     */
    bool connect(unsigned long timeoutMs = 10000);
    
    /**
     * @brief Disconnect from WiFi
     */
    void disconnect();
    
    /**
     * @brief Check if WiFi is connected
     */
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    
    /**
     * @brief Get local IP address
     */
    String getLocalIP() const { return WiFi.localIP().toString(); }
    
    /**
     * @brief Get MAC address
     */
    String getMacAddress() const { return WiFi.macAddress(); }
    
    /**
     * @brief Get RSSI (signal strength)
     */
    int getRSSI() const { return WiFi.RSSI(); }
    
    /**
     * @brief Set callbacks for connection events
     */
    void setCallbacks(void (*onConnect)(), void (*onDisconnect)());
    
    /**
     * @brief Print WiFi status to Serial
     */
    void printStatus();
};

#endif // WIFI_SERVICE_H