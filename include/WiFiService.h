#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <Arduino.h>
#include <WiFiManager.h>

class WiFiService {
private:
    WiFiManager wm;

public:
    WiFiService() {}

    // Ahora 'begin' inicia el portal de configuración automático
    void begin(const String& unusedSSID = "", const String& unusedPassword = "") {
        // En lugar de conectar manualmente, dejamos que WiFiManager lo haga
        // "Clair-AutoConnect" es el nombre del punto de acceso si falla la conexión
        wm.autoConnect("Clair-AutoConnect");
    }

    void update() {
        // WiFiManager gestiona la reconexión automáticamente en segundo plano.
        // Si necesitas monitorear algo, puedes dejarlo vacío o añadir lógica de watchdog.
    }

    void disconnect() {
        WiFi.disconnect(true);
    }

    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    String getLocalIP() const { return WiFi.localIP().toString(); }
    int getRSSI() const { return WiFi.RSSI(); }
    
    // Estos métodos ya no necesitan lógica compleja
    void printStatus() {
        Serial.printf("[WiFi] Status: %s | IP: %s | RSSI: %d dBm\n", 
                      isConnected() ? "CONNECTED" : "DISCONNECTED",
                      getLocalIP().c_str(), getRSSI());
    }
};
#endif