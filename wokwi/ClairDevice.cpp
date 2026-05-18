#include "ClairDevice.h"
#include <Arduino.h>

// Constructor con struct
ClairDevice::ClairDevice(const ClairPins& pins, 
                         unsigned long scd41Interval,
                         unsigned long pmsInterval,
                         unsigned long reportInterval,
                         int displaySda,
                         int displayScl)
    : scd41Device(pins.scd41_sda, pins.scd41_scl, scd41Interval),
      pms5003Device(pins.pms_rx, pins.pms_tx, pins.pms_set, pins.pms_reset, pmsInterval),
      display(128, 64, displaySda, displayScl, 0x3C, this),
      warningLed(pins.rgb_red, pins.rgb_green, pins.rgb_blue, false, false, false, this, true),
      lastReportTime(0),
      reportInterval(reportInterval),
      allSensorsReady(false),
      lastLedBlink(0),
      ledBlinkState(false) {}

// Constructor with individual parameters
ClairDevice::ClairDevice(int sda, int scl, int rx, int tx, int set, int reset,
                         unsigned long scd41Interval,
                         unsigned long pmsInterval,
                         unsigned long reportInterval,
                         int displaySda,
                         int displayScl,
                         int rgbRedPin,
                         int rgbGreenPin,
                         int rgbBluePin)
    : scd41Device(sda, scl, scd41Interval),
      pms5003Device(rx, tx, set, reset, pmsInterval),
      display(128, 64, displaySda, displayScl, 0x3C, this),
      warningLed(rgbRedPin, rgbGreenPin, rgbBluePin, false, false, false, this, true),
      lastReportTime(0),
      reportInterval(reportInterval),
      allSensorsReady(false),
      lastLedBlink(0),
      ledBlinkState(false) {}

// Inicializar sistema
bool ClairDevice::begin() {
    bool scd41Ok = scd41Device.getSensor().begin();
    bool pmsOk = pms5003Device.getSensor().begin();
    
    allSensorsReady = scd41Ok && pmsOk;
    
    if (!scd41Ok) {
        Serial.println("SCD41 sensor initialization FAILED");
    }
    
    if (!pmsOk) {
        Serial.println("PMS5003 sensor initialization FAILED");
    }
    
    // Initialize display
    if (!display.begin()) {
        Serial.println("OLED display initialization FAILED");
    }
    display.setSleepTimeout(30000);  // 30 second timeout
    
    
    warningLed.off();

    
    if (allSensorsReady) {
        forceReport();
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
    
    // Update WiFi
    wifi.update();
    
    // Send data to the cloud if WiFi is connected
    if (wifi.isConnected() && cloud.isEnabled()) {
        if (cloud.sendDataThrottled(currentData)) {
            // Success is handled inside sendDataThrottled
        }
    }
    
    unsigned long now = millis();
    if (now - lastReportTime >= reportInterval) {
        generateUnifiedReport();
        lastReportTime = now;
    }
}

void ClairDevice::updateAirQualityData() {
    if (scd41Device.getSensor().isInitialized()) {
        currentData.airQuality.co2 = scd41Device.getSensor().getCO2();
        currentData.airQuality.temperature = scd41Device.getSensor().getTemperature();
        currentData.airQuality.humidity = scd41Device.getSensor().getHumidity();
        currentData.airQuality.valid = true;
    } else {
        currentData.airQuality.valid = false;
    }
    
    // Evaluate overall air quality status
    currentData.evaluateStatus(thresholds);
}

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
    
    // Evaluate overall air quality status
    currentData.evaluateStatus(thresholds);
}

void ClairDevice::updateWarningLed() {
    if (currentData.status == CRITICAL) {
        unsigned long now = millis();
        if (now - lastLedBlink > 250) {
            ledBlinkState = !ledBlinkState;
            warningLed.setColor(ledBlinkState, false, false);
            lastLedBlink = now;
        }
    } else if (currentData.status == MODERATE) {
        unsigned long now = millis();
        if (now - lastLedBlink > 1000) {
            ledBlinkState = !ledBlinkState;
            warningLed.setColor(ledBlinkState, ledBlinkState, false);
            lastLedBlink = now;
        }
    } else {
        warningLed.setColor(false, true, false);
    }
}

// Generate unified report and update display
void ClairDevice::generateUnifiedReport() {
    currentData.timestamp = millis();
    currentData.print();
    
    DisplayData displayData;

    displayData.airQuality.co2 = currentData.airQuality.co2;
    displayData.airQuality.temperature = currentData.airQuality.temperature;
    displayData.airQuality.humidity = currentData.airQuality.humidity;
    displayData.airQuality.valid = currentData.airQuality.valid;
    displayData.airQuality.statusLabel = currentData.statusLabel;

    displayData.particulateMatter.pm1_0 = currentData.particulateMatter.pm1_0;
    displayData.particulateMatter.pm2_5 = currentData.particulateMatter.pm2_5;
    displayData.particulateMatter.pm10 = currentData.particulateMatter.pm10;
    displayData.particulateMatter.valid = currentData.particulateMatter.valid;

    displayData.aqi.value = currentData.airQualityIndex.aqi;
    displayData.aqi.category = currentData.airQualityIndex.category;

    display.updateData(displayData);
}

void ClairDevice::on(Event event) {
    if (event.id == SCD41Sensor::DATA_READY_EVENT_ID) {
        updateAirQualityData();
        Serial.println("New air quality data");
    }
    else if (event.id == PMS5003Sensor::DATA_READY_EVENT_ID) {
        updateParticulateMatterData();
        Serial.println("New particulate matter data");
    }
}

void ClairDevice::handle(Command command) {
    if (command.id == CLAIR_REPORT_COMMAND) {
        Serial.println("Force report");
        forceReport();
    }
    else if (command.id == CLAIR_CALIBRATE_COMMAND) {
        Serial.println("Calibration");
        scd41Device.getSensor().recalibrate(400);
    }
    else if (command.id == CLAIR_RESET_COMMAND) {
        Serial.println("Reset");
        pms5003Device.getSensor().reset();
    }
    else if (command.id == OLEDDisplay::DISPLAY_ON_COMMAND ||
             command.id == OLEDDisplay::DISPLAY_OFF_COMMAND ||
             command.id == OLEDDisplay::DISPLAY_SLEEP_COMMAND ||
             command.id == OLEDDisplay::DISPLAY_WAKE_COMMAND) {
        display.handle(command);
    }
}

ClairData ClairDevice::getCurrentData() {
    updateAirQualityData();
    updateParticulateMatterData();
    return currentData;
}

void ClairDevice::forceReport() {
    updateAirQualityData();
    updateParticulateMatterData();
    generateUnifiedReport();
}

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
