/**
 * @file ClairSystem.ino
 * @brief Main sketch with WiFi and Cloud
 */

#include "ClairDevice.h"

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

// Simplificado - una sola URL
#define EDGE_BASE_URL ""
#define HARDWARE_ID "CLAIR-0001"
#define DEVICE_SECRET ""

#define CLOUD_SEND_INTERVAL 10000      // 15 segundos - telemetría al Cloud
#define REMOTE_POLL_INTERVAL 10000     // 10 segundos - comandos desde Edge

#define CLAIR_SIMULATION_MODE false

ClairDevice clair;

void printBanner() {
    Serial.println("\n==================================================");
    Serial.println("     Environmental Monitoring System v1.3");
    Serial.println("==================================================\n");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    printBanner();
    
    clair.begin();
    clair.setupWiFi(WIFI_SSID, WIFI_PASSWORD);
    clair.beginNTP("pool.ntp.org", -18000);  // -18000 = UTC-5
    
    // IMPORTANTE: Edge para enviar TELEMETRÍA y recibir COMANDOS
    clair.setupEdge(EDGE_BASE_URL, HARDWARE_ID, DEVICE_SECRET, CLOUD_SEND_INTERVAL, REMOTE_POLL_INTERVAL);
    
    clair.setSimulationEnabled(CLAIR_SIMULATION_MODE);
}

void loop() {
    clair.update();
    delay(50);
    
    // Mostrar estado del modo standby si cambió
    static bool lastStandbyState = false;
    if (clair.isStandbyMode() != lastStandbyState) {
        lastStandbyState = clair.isStandbyMode();
        Serial.print("Standby mode: ");
        Serial.println(lastStandbyState ? "ACTIVE" : "INACTIVE");
    }
}