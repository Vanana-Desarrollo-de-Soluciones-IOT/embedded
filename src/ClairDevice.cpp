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
      simulationEnabled(false),
      simulationStartTime(0),
      lastDisplayedStatus(OPTIMAL),
      displayInitialized(false),
      initState(INIT_NOT_STARTED),
      initStartTime(0),
      initTimeoutOccurred(false) {}

// Inicializar sistema
bool ClairDevice::begin() {
    // Iniciar proceso de inicialización NO bloqueante
    initState = INIT_STARTING_SENSORS;
    initStartTime = millis();
    initTimeoutOccurred = false;
    allSensorsReady = false;
    
    Serial.println("[ClairDevice] Starting non-blocking initialization...");
    
    // Iniciar sensores (son operaciones rápidas, no bloqueantes)
    scd41Device.getSensor().begin();
    pms5003Device.getSensor().begin();
    display.begin();
    
    // Pasar al estado de espera
    initState = INIT_WAITING_SENSORS;
    
    // No esperar aquí - la inicialización continuará en update()
    return true;  // Siempre retorna true, la inicialización continúa en background
}

// NUEVO método: Llamar desde update() hasta que INIT_COMPLETE
void ClairDevice::updateInitialization() {
    if (initState == INIT_COMPLETE || initState == INIT_PARTIAL) {
        return;  // Ya terminó
    }
    
    unsigned long now = millis();
    
    if (initState == INIT_WAITING_SENSORS) {
        bool scd41Ready = scd41Device.getSensor().isInitialized();
        bool pmsReady = pms5003Device.getSensor().isInitialized();
        
        if (scd41Ready && pmsReady) {
            initState = INIT_COMPLETE;
            allSensorsReady = true;
            Serial.println("[ClairDevice] All sensors initialized successfully!");
        }
        else if (now - initStartTime >= INIT_TIMEOUT_MS) {
            // Timeout - continuar con los sensores que funcionan
            initTimeoutOccurred = true;
            
            if (scd41Ready || pmsReady) {
                initState = INIT_PARTIAL;
                allSensorsReady = false;  // No todos están listos, pero podemos operar
                Serial.print("[ClairDevice] Initialization partial. SCD41: ");
                Serial.print(scd41Ready ? "OK" : "FAIL");
                Serial.print(", PMS5003: ");
                Serial.println(pmsReady ? "OK" : "FAIL");
            } else {
                initState = INIT_PARTIAL;
                Serial.println("[ClairDevice] WARNING: No sensors initialized!");
            }
        }
    }
}

const char* ClairDevice::getInitStateString() const {
    switch (initState) {
        case INIT_NOT_STARTED: return "NOT_STARTED";
        case INIT_STARTING_SENSORS: return "STARTING";
        case INIT_WAITING_SENSORS: return "WAITING";
        case INIT_COMPLETE: return "COMPLETE";
        case INIT_PARTIAL: return "PARTIAL";
        default: return "UNKNOWN";
    }
}

// Actualizar todos los sensores
void ClairDevice::update() {
    // NUEVO: Gestionar inicialización no bloqueante
    updateInitialization();
    
    // Solo procesar sensores si la inicialización avanzó lo suficiente
    if (initState == INIT_COMPLETE || initState == INIT_PARTIAL) {
        if (simulationEnabled) {
            updateSimulationData();
        } else {
            scd41Device.update();
            pms5003Device.update();
            updateAirQualityData();
            updateParticulateMatterData();
        }
        
        updateWarningLed();
        
        // Resto del código igual...
        if (!displayInitialized || currentData.status != lastDisplayedStatus) {
            refreshDisplay();
            lastDisplayedStatus = currentData.status;
            displayInitialized = true;
        }
        display.autoPowerManagement();
        
        wifi.update();
        
        if (wifi.isConnected() && cloud.isEnabled()) {
            cloud.sendDataThrottled(currentData);
        }
        
        unsigned long now = millis();
        if (now - lastReportTime >= reportInterval) {
            generateUnifiedReport();
            lastReportTime = now;
        }
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
        warningLed.setColor(true, false, false);
    } else if (currentData.status == MODERATE) {
        warningLed.setColor(true, true, false);
    } else {
        warningLed.setColor(false, true, false);
    }
}

// Generate unified report and update display
void ClairDevice::generateUnifiedReport() {
    currentData.timestamp = millis();
    currentData.print();
    refreshDisplay();
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

void ClairDevice::refreshDisplay() {
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

void ClairDevice::updateSimulationData() {
    static const unsigned long PHASE_DURATIONS[] = {20000UL, 8000UL, 3000UL};
    static const unsigned long CYCLE_MS = PHASE_DURATIONS[0] + PHASE_DURATIONS[1] + PHASE_DURATIONS[2];
    unsigned long now = millis();
    unsigned long elapsed = (now - simulationStartTime) % CYCLE_MS;
    unsigned long phase = 0;

    if (elapsed >= PHASE_DURATIONS[0] + PHASE_DURATIONS[1]) {
        phase = 2;
    } else if (elapsed >= PHASE_DURATIONS[0]) {
        phase = 1;
    }

    currentData.timestamp = now;
    currentData.airQuality.valid = true;
    currentData.particulateMatter.valid = true;

    if (phase == 0) {
        currentData.airQuality.co2 = 480 + (now / 1000) % 120;
        currentData.airQuality.temperature = 23.5 + ((now / 5000) % 3) * 0.4;
        currentData.airQuality.humidity = 42.0 + ((now / 3000) % 4) * 1.0;
        currentData.particulateMatter.pm1_0 = 3 + ((now / 4000) % 3);
        currentData.particulateMatter.pm2_5 = 6 + ((now / 3000) % 4);
        currentData.particulateMatter.pm10 = 12 + ((now / 2500) % 5);
    } else if (phase == 1) {
        currentData.airQuality.co2 = 880 + (now / 800) % 180;
        currentData.airQuality.temperature = 25.0 + ((now / 4000) % 3) * 0.5;
        currentData.airQuality.humidity = 54.0 + ((now / 2500) % 5) * 0.8;
        currentData.particulateMatter.pm1_0 = 9 + ((now / 2000) % 4);
        currentData.particulateMatter.pm2_5 = 24 + ((now / 2500) % 10);
        currentData.particulateMatter.pm10 = 55 + ((now / 2000) % 12);
    } else {
        currentData.airQuality.co2 = 1500 + (now / 700) % 500;
        currentData.airQuality.temperature = 27.0 + ((now / 3000) % 3) * 0.6;
        currentData.airQuality.humidity = 72.0 + ((now / 2000) % 4) * 1.2;
        currentData.particulateMatter.pm1_0 = 22 + ((now / 1500) % 8);
        currentData.particulateMatter.pm2_5 = 55 + ((now / 2000) % 20);
        currentData.particulateMatter.pm10 = 145 + ((now / 1500) % 30);
    }

    currentData.calculateAQI();
    currentData.evaluateStatus(thresholds);
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

void ClairDevice::setupCloud(const String& endpoint, const String& hardwareId, const String& deviceSecret, unsigned long interval) {
    cloud.begin(endpoint, hardwareId, deviceSecret, interval);
}

void ClairDevice::setSimulationEnabled(bool enabled) {
    simulationEnabled = enabled;
    simulationStartTime = millis();
    displayInitialized = false;
}
