/**
 * @file ClairSystem.ino
 * @brief Main sketch with WiFi and Cloud
 */

#include "ClairDevice.h"

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

// Configuración del Edge (para comandos)
#define EDGE_ENDPOINT ""

// Configuración del Cloud (para telemetría) 
#define CLOUD_ENDPOINT ""

#define HARDWARE_ID "CLAIR-0001"
#define DEVICE_SECRET "DWWPmhQcjF7quB-asAvHmWnF47eY0Bvqh26ohQ9e9Vk"

#define CLOUD_SEND_INTERVAL 15000      // 15 segundos - telemetría al Cloud
#define REMOTE_POLL_INTERVAL 10000     // 10 segundos - comandos desde Edge

#define CLAIR_SIMULATION_MODE false

ClairDevice clair;

void printBanner() {
    Serial.println("\n");
    Serial.println("   ██████╗██╗      █████╗ ██╗██████╗ ");
    Serial.println("  ██╔════╝██║     ██╔══██╗██║██╔══██╗");
    Serial.println("  ██║     ██║     ███████║██║██████╔╝");
    Serial.println("  ██║     ██║     ██╔══██║██║██╔══██╗");
    Serial.println("  ╚██████╗███████╗██║  ██║██║██║  ██║");
    Serial.println("   ╚═════╝╚══════╝╚═╝  ╚═╝╚═╝╚═╝  ╚═╝");
    Serial.println("                                         ");
    Serial.println("   Environmental Monitoring System v1.2");
    Serial.println("   =====================================\n");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    printBanner();
    
    clair.begin();
    clair.setupWiFi(WIFI_SSID, WIFI_PASSWORD);
    clair.beginNTP("pool.ntp.org", -18000);  // -18000 = UTC-5
    
    // IMPORTANTE: Cloud para enviar TELEMETRÍA
    clair.setupCloud(CLOUD_ENDPOINT, HARDWARE_ID, DEVICE_SECRET, CLOUD_SEND_INTERVAL);
    
    // IMPORTANTE: Remote Commands para recibir COMANDOS
    clair.setupRemoteCommands(EDGE_ENDPOINT, HARDWARE_ID, DEVICE_SECRET, REMOTE_POLL_INTERVAL);
    
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