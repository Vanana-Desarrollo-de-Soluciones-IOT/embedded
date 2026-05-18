/**
 * @file ClairSystem.ino
 * @brief Main sketch with WiFi and Cloud
 */

#include "ClairDevice.h"

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

#define CLOUD_ENDPOINT "https://iot-edge-6785-im.free.beeceptor.com/api/v1/data-records"
#define DEVICE_ID "CLAIR001"

ClairDevice clair;

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (clair.begin()) {
    } else {
    }

    clair.setupWiFi(WIFI_SSID, WIFI_PASSWORD);
    clair.setupCloud(CLOUD_ENDPOINT, DEVICE_ID, 30000);
}

void loop() {
    clair.update();
    delay(50);
}
