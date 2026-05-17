#include "ClairDevice.h"
#include <Arduino.h>

// Constructor con struct
ClairDevice::ClairDevice(const ClairPins& pins, 
                         unsigned long scd41Interval,
                         unsigned long pmsInterval,
                         unsigned long reportInterval,
                         int displaySda,
                         int displayScl,
                         int ledPin)
    : scd41Device(pins.scd41_sda, pins.scd41_scl, scd41Interval),
      pms5003Device(pins.pms_rx, pins.pms_tx, pins.pms_set, pins.pms_reset, pmsInterval),
      display(128, 64, displaySda, displayScl, 0x3C, this),
      warningLed(ledPin, false, this),  
      lastReportTime(0),
      reportInterval(reportInterval),
      allSensorsReady(false),
      lastLedBlink(0),
      ledBlinkState(false) {}

// Constructor con parámetros individuales
ClairDevice::ClairDevice(int sda, int scl, int rx, int tx, int set, int reset,
                         unsigned long scd41Interval,
                         unsigned long pmsInterval,
                         unsigned long reportInterval,
                         int displaySda,
                         int displayScl,
                         int ledPin)
    : scd41Device(sda, scl, scd41Interval),
      pms5003Device(rx, tx, set, reset, pmsInterval),
      display(128, 64, displaySda, displayScl, 0x3C, this),
      warningLed(ledPin, false, this),  
      lastReportTime(0),
      reportInterval(reportInterval),
      allSensorsReady(false),
      lastLedBlink(0),
      ledBlinkState(false) {}

// Inicializar sistema
bool ClairDevice::begin() {
    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println("║     CLAIR ENVIRONMENTAL SYSTEM v1.0     ║");
    Serial.println("║     Initializing sensors...              ║");
    Serial.println("╚══════════════════════════════════════════╝\n");
    
    bool scd41Ok = scd41Device.getSensor().begin();
    bool pmsOk = pms5003Device.getSensor().begin();
    
    allSensorsReady = scd41Ok && pmsOk;
    
    if (!scd41Ok) {
        Serial.println("❌ SCD41 Sensor initialization FAILED");
        Serial.println("   → Check I2C connections (SDA=21, SCL=22)");
    } else {
        Serial.println("✅ SCD41 Sensor initialized");
    }
    
    if (!pmsOk) {
        Serial.println("❌ PMS5003 Sensor initialization FAILED");
        Serial.println("   → Check UART connections (RX=16, TX=17)");
        Serial.println("   → Check power (5V required)");
    } else {
        Serial.println("✅ PMS5003 Sensor initialized");
    }
    
    // Inicializar display
    if (!display.begin()) {
        Serial.println("⚠️  OLED Display initialization FAILED");
        Serial.println("   → Check I2C connections (SDA=21, SCL=22)");
    } else {
        Serial.println("✅ OLED Display initialized");
        display.setSleepTimeout(30000);  // 30 segundos de timeout
    }
    
    
    // Prueba de LED
    warningLed.setState(true);
    delay(500);
    warningLed.setState(false);    

    
    if (allSensorsReady) {
        Serial.println("\n╔══════════════════════════════════════════╗");
        Serial.println("║     SYSTEM READY - Monitoring Started    ║");
        Serial.println("╚══════════════════════════════════════════╝\n");
        forceReport();
    } else {
        Serial.println("\n⚠️  SYSTEM PARTIALLY OPERATIONAL");
        Serial.println("   Some sensors failed to initialize\n");
    }
    
    return allSensorsReady;
}

// Actualizar todos los sensores
void ClairDevice::update() {
    scd41Device.update();
    pms5003Device.update();
    
    updateAirQualityData();
    updateParticulateMatterData();
    
    updateWarningLed();
    display.autoPowerManagement();
    
    // Actualizar WiFi
    wifi.update();
    
    // Enviar datos a la nube si WiFi está conectado
    if (wifi.isConnected() && cloud.isEnabled()) {
        if (cloud.sendDataThrottled(currentData)) {
            // Éxito - ya se maneja dentro de sendDataThrottled
        }
    }
    
    unsigned long now = millis();
    if (now - lastReportTime >= reportInterval) {
        generateUnifiedReport();
        lastReportTime = now;
    }
}

// Actualizar datos de calidad del aire
void ClairDevice::updateAirQualityData() {
    if (scd41Device.getSensor().isInitialized()) {
        currentData.airQuality.co2 = scd41Device.getSensor().getCO2();
        currentData.airQuality.temperature = scd41Device.getSensor().getTemperature();
        currentData.airQuality.humidity = scd41Device.getSensor().getHumidity();
        currentData.airQuality.valid = true;
    } else {
        currentData.airQuality.valid = false;
    }
    
    // Evaluar estado general de calidad del aire
    currentData.evaluateStatus(thresholds);
}

// Actualizar datos de partículas
void ClairDevice::updateParticulateMatterData() {
    if (pms5003Device.getSensor().isInitialized()) {
        PMS5003Data pmsData = pms5003Device.getSensor().getData();
        currentData.particulateMatter.pm1_0 = pmsData.pm1_0;
        currentData.particulateMatter.pm2_5 = pmsData.pm2_5;
        currentData.particulateMatter.pm10 = pmsData.pm10;
        currentData.particulateMatter.valid = pmsData.valid;
        
        if (pmsData.valid) {
            currentData.calculateAQI();
        }
    } else {
        currentData.particulateMatter.valid = false;
    }
    
    // Evaluar estado general de calidad del aire
    currentData.evaluateStatus(thresholds);
}

void ClairDevice::updateWarningLed() {
    if (currentData.status == CRITICAL) {
        // Parpadeo rápido cuando es CRITICAL
        unsigned long now = millis();
        if (now - lastLedBlink > 250) {  // Parpadeo cada 250ms
            ledBlinkState = !ledBlinkState;
            warningLed.setState(ledBlinkState);
            lastLedBlink = now;
        }
    } else if (currentData.status == MODERATE) {
        // Parpadeo lento cuando es MODERATE
        unsigned long now = millis();
        if (now - lastLedBlink > 1000) {  // Parpadeo cada 1 segundo
            ledBlinkState = !ledBlinkState;
            warningLed.setState(ledBlinkState);
            lastLedBlink = now;
        }
    } else {  // OPTIMAL
        // LED apagado cuando es OPTIMAL
        if (warningLed.getState()) {
            warningLed.setState(false);
        }
    }
}

// Generar reporte unificado y actualizar display
void ClairDevice::generateUnifiedReport() {
    currentData.timestamp = millis();
    currentData.print();
    
    // Preparar datos para el display
    DisplayData displayData;
    
    // Copiar datos de calidad del aire
    displayData.airQuality.co2 = currentData.airQuality.co2;
    displayData.airQuality.temperature = currentData.airQuality.temperature;
    displayData.airQuality.humidity = currentData.airQuality.humidity;
    displayData.airQuality.valid = currentData.airQuality.valid;
    displayData.airQuality.statusLabel = currentData.statusLabel;  // Usar el nuevo status
    
    // Copiar datos de partículas
    displayData.particulateMatter.pm1_0 = currentData.particulateMatter.pm1_0;
    displayData.particulateMatter.pm2_5 = currentData.particulateMatter.pm2_5;
    displayData.particulateMatter.pm10 = currentData.particulateMatter.pm10;
    displayData.particulateMatter.valid = currentData.particulateMatter.valid;
    
    // Copiar AQI
    displayData.aqi.value = currentData.airQualityIndex.aqi;
    displayData.aqi.category = currentData.airQualityIndex.category;
    
    // Actualizar display
    display.updateData(displayData);
}

// Manejador de eventos
void ClairDevice::on(Event event) {
    if (event.id == SCD41Sensor::DATA_READY_EVENT_ID) {
        updateAirQualityData();
        Serial.println("[Clair] New air quality data received");
    }
    else if (event.id == PMS5003Sensor::DATA_READY_EVENT_ID) {
        updateParticulateMatterData();
        Serial.println("[Clair] New particulate matter data received");
    }
}

// Manejador de comandos
void ClairDevice::handle(Command command) {
    if (command.id == CLAIR_REPORT_COMMAND) {
        Serial.println("[Clair] Force report command received");
        forceReport();
    }
    else if (command.id == CLAIR_CALIBRATE_COMMAND) {
        Serial.println("[Clair] Calibration command received");
        scd41Device.getSensor().recalibrate(400);
    }
    else if (command.id == CLAIR_RESET_COMMAND) {
        Serial.println("[Clair] Reset command received");
        pms5003Device.getSensor().reset();
    }
    // Forward display commands
    else if (command.id == OLEDDisplay::DISPLAY_ON_COMMAND ||
             command.id == OLEDDisplay::DISPLAY_OFF_COMMAND ||
             command.id == OLEDDisplay::DISPLAY_SLEEP_COMMAND ||
             command.id == OLEDDisplay::DISPLAY_WAKE_COMMAND) {
        display.handle(command);
    }
}

// Obtener datos actuales
ClairData ClairDevice::getCurrentData() {
    updateAirQualityData();
    updateParticulateMatterData();
    return currentData;
}

// Forzar reporte
void ClairDevice::forceReport() {
    updateAirQualityData();
    updateParticulateMatterData();
    generateUnifiedReport();
}

// IMPLEMENTACIÓN DEL MÉTODO FALTANTE
String ClairDevice::getAirQualityLabel(int co2) {
    if (co2 < 400) return "Excellent";
    if (co2 < 600) return "Good";
    if (co2 < 800) return "Normal";
    if (co2 < 1000) return "Moderate";
    if (co2 < 1500) return "Poor";
    return "Bad";
}

void ClairDevice::setupWiFi(const String& ssid, const String& password) {
    wifi.begin(ssid, password);
}

void ClairDevice::setupCloud(const String& endpoint, const String& deviceId, unsigned long interval) {
    cloud.begin(endpoint, deviceId, interval);
}
