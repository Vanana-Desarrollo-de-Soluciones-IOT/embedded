/**
 * @file ClairSystem.ino
 * @brief Main sketch with WiFi and Cloud
 */

#include "ClairDevice.h"

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

#define CLOUD_ENDPOINT ""
#define DEVICE_ID "CLAIR-0001"
#define DEVICE_SECRET "fOIP-dIzEnXmirN8edZyY91f98wcskUFAAI_6E78uw8"
#define CLOUD_SEND_INTERVAL 10000 // 30 seconds

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

