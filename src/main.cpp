/**
 * @file ClairSystem.ino
 * @brief Main sketch with WiFi and Cloud
 */

#include "ClairDevice.h"

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

#define CLOUD_ENDPOINT "https://perraaaaa.free.beeceptor.com/api/v1/device/telemetry"
#define DEVICE_ID "CLAIR-0001"
#define DEVICE_SECRET "fOIP-dIzEnXmirN8edZyY91f98wcskUFAAI_6E78uw8"
#define CLOUD_SEND_INTERVAL 10000 // 10 seconds

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
    
    // begin() ahora es NO bloqueante
    if (clair.begin()) {
        Serial.println("System initialization started in background");
    } else {
        Serial.println("Failed to start system initialization");
    }
    
    clair.setupWiFi(WIFI_SSID, WIFI_PASSWORD);
    clair.setupCloud(CLOUD_ENDPOINT, DEVICE_ID, DEVICE_SECRET, CLOUD_SEND_INTERVAL);
    clair.setSimulationEnabled(CLAIR_SIMULATION_MODE);
}

void loop() {
    clair.update();  // Aquí se completa la inicialización gradualmente
    delay(50);
    
    // Opcional: monitorear estado de inicialización
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 5000 && !clair.isInitializationComplete()) {
        Serial.print("Initialization state: ");
        Serial.println(clair.getInitStateString());
        lastStatusPrint = millis();
    }
}

