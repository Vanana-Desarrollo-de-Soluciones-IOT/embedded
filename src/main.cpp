/**
 * @file ClairSystem.ino
 * @brief Main sketch with WiFi and Cloud
 */

#include "ClairDevice.h"

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

#define CLOUD_ENDPOINT "https://perraaaaa.free.beeceptor.com/"
#define DEVICE_ID "CLAIR001"
#define DEVICE_SECRET "your-device-secret"
#define CLOUD_SEND_INTERVAL 30000 // 30 seconds

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

    printBanner();//optional

    if (clair.begin()) {
        Serial.println("System ready");
    } else {
        Serial.println("System partially operational");
    }

    clair.setupWiFi(WIFI_SSID, WIFI_PASSWORD);
    clair.setupCloud(CLOUD_ENDPOINT, DEVICE_ID, DEVICE_SECRET, CLOUD_SEND_INTERVAL);
    clair.setSimulationEnabled(CLAIR_SIMULATION_MODE);
}

void loop() {
    clair.update();
    delay(50);
}

