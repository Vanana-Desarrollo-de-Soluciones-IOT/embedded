/**
 * @file ClairSystem.ino
 * @brief Main sketch con WiFi y Cloud
 */

#include "ClairDevice.h"

// Configuración WiFi
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

// Configuración Cloud
#define CLOUD_ENDPOINT "https://iot-edge-6785-im.free.beeceptor.com/api/v1/data-records"
#define DEVICE_ID "CLAIR001"

// Crear instancia
ClairDevice clair;

unsigned long lastStatusPrint = 0;
const unsigned long STATUS_PRINT_INTERVAL = 60000;  // Cada minuto

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    printBanner();
    
    // Inicializar sistema Clair
    if (clair.begin()) {
        Serial.println("✅ System ready!\n");
    } else {
        Serial.println("⚠️ System partially operational\n");
    }
    
    // Configurar WiFi
    Serial.println("\n📡 Configuring WiFi...");
    clair.setupWiFi(WIFI_SSID, WIFI_PASSWORD);
    
    // Configurar Cloud (solo se enviará cuando WiFi esté conectado)
    Serial.println("\n☁️ Configuring Cloud Service...");
    clair.setupCloud(CLOUD_ENDPOINT, DEVICE_ID, 30000);  // Enviar cada 30 segundos
    
    printHelp();
}

void loop() {
    clair.update();
    processSerialCommands();
    
    // Mostrar estado cada minuto
    unsigned long now = millis();
    if (now - lastStatusPrint >= STATUS_PRINT_INTERVAL) {
        printStatus();
        lastStatusPrint = now;
    }
    
    delay(50);
}

void processSerialCommands() {
    if (Serial.available()) {
        char cmd = tolower(Serial.read());
        
        switch (cmd) {
            case 'r':  // Force report
                clair.forceReport();
                break;
                
            case 'l':  // Test LED
                Serial.println("\n💡 Testing warning LED...");
                clair.getWarningLed().setState(true);
                delay(1000);
                clair.getWarningLed().setState(false);
                Serial.println("   LED test complete\n");
                break;
                
            case 's':  // Show status
                printStatus();
                break;
                
            case 'w':  // WiFi status
                clair.isWiFiConnected() ? 
                    Serial.printf("\n📡 WiFi Connected - IP: %s\n", clair.getWiFiIP().c_str()) :
                    Serial.println("\n📡 WiFi Disconnected\n");
                break;
                
            case 'c':  // Cloud toggle
                if (clair.isCloudEnabled()) {
                    clair.setCloudEnabled(false);
                    Serial.println("\n☁️ Cloud sending DISABLED");
                } else {
                    clair.setCloudEnabled(true);
                    Serial.println("\n☁️ Cloud sending ENABLED");
                }
                break;
                
            case 'h':  // Help
                printHelp();
                break;
                
            default:
                Serial.printf("\n❓ Unknown command: '%c'\n", cmd);
                Serial.println("   Type 'h' for help\n");
                break;
        }
    }
}

void printStatus() {
    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println("║              SYSTEM STATUS               ║");
    Serial.println("╠══════════════════════════════════════════╣");
    
    // WiFi Status
    if (clair.isWiFiConnected()) {
        Serial.printf("║ WiFi:     Connected (%s)║\n", clair.getWiFiIP().c_str());
    } else {
        Serial.println("║ WiFi:     Disconnected                    ║");
    }
    
    // Cloud Status
    Serial.printf("║ Cloud:    %s                              ║\n", 
                  clair.isCloudEnabled() ? "Enabled" : "Disabled");
    
    // Air Quality Status
    Serial.printf("║ Air Q:    %s                          ║\n", 
                  clair.getCurrentStatusLabel().c_str());
    
    Serial.println("╚══════════════════════════════════════════╝\n");
}

void printBanner() {
    Serial.println("\n");
    Serial.println("   ██████╗██╗      █████╗ ██╗██████╗ ");
    Serial.println("  ██╔════╝██║     ██╔══██╗██║██╔══██╗");
    Serial.println("  ██║     ██║     ███████║██║██████╔╝");
    Serial.println("  ██║     ██║     ██╔══██║██║██╔══██╗");
    Serial.println("  ╚██████╗███████╗██║  ██║██║██║  ██║");
    Serial.println("   ╚═════╝╚══════╝╚═╝  ╚═╝╚═╝╚═╝  ╚═╝");
    Serial.println("                                         ");
    Serial.println("   Environmental Monitoring System v1.0");
    Serial.println("   =====================================\n");
}

void printHelp() {
    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println("║           CLAIR SYSTEM COMMANDS          ║");
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.println("║  'r'  → Force unified report            ║");
    Serial.println("║  'l'  → Test warning LED                ║");
    Serial.println("║  's'  → Show system status              ║");
    Serial.println("║  'w'  → Show WiFi status                ║");
    Serial.println("║  'c'  → Toggle cloud sending            ║");
    Serial.println("║  'h'  → Show this help                  ║");
    Serial.println("╚══════════════════════════════════════════╝\n");
}