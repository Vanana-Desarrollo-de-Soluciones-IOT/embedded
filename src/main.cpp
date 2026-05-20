/**
 * @file ClairSystem.ino
 * @brief Main sketch with WiFi and Cloud
 */

#include "ClairDevice.h"

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

// Simplificado - una sola URL 
#define EDGE_BASE_URL "https://tu-edge-server.com"  // Cambiar por tu URL real
#define HARDWARE_ID "CLAIR-0001"
#define DEVICE_SECRET "H7HRqysSxsaKdkRYJoB46goGucCAlahPF09XEO4QNDM"

#define CLOUD_SEND_INTERVAL 10000      // 10 segundos - telemetría al Cloud
#define REMOTE_POLL_INTERVAL 10000     // 10 segundos - comandos desde Edge

#define CLAIR_SIMULATION_MODE false

ClairDevice clair;
ClairDevice* g_clairDevice = &clair;  // Instancia global para callbacks estáticos

void printBanner() {
    Serial.println("\n==================================================");
    Serial.println("     Environmental Monitoring System v1.4");
    Serial.println("==================================================\n");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    printBanner();
    
    clair.begin();
    
    // PRIMERO: Conectar WiFi (rápido)
    clair.setupWiFi(WIFI_SSID, WIFI_PASSWORD);
    
    // SEGUNDO: Iniciar NTP (rápido, no bloqueante)
    clair.beginNTP("pool.ntp.org", -18000);
    
    // TERCERO: Configurar Edge Service
    clair.setupEdge(EDGE_BASE_URL, HARDWARE_ID, DEVICE_SECRET, 
                    CLOUD_SEND_INTERVAL, REMOTE_POLL_INTERVAL);
    
    // CUARTO: Los sensores se inicializan en background
    clair.setSimulationEnabled(CLAIR_SIMULATION_MODE);
    
    // El sistema ya está respondiendo aunque los sensores no estén listos
    Serial.println("[Main] Setup complete - system running with partial data if needed");
}

void loop() {
    clair.update();
    delay(50);
    
    // Mostrar estado del modo standby si cambió
    static bool lastStandbyState = false;
    if (clair.isStandbyMode() != lastStandbyState) {
        lastStandbyState = clair.isStandbyMode();
        Serial.print("[Main] Standby mode: ");
        Serial.println(lastStandbyState ? "ACTIVE" : "INACTIVE");
    }
}