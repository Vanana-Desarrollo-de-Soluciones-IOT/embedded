// ClairDevice.cpp - Versión limpia solo con LED simple

#include "ClairDevice.h"
#include <Arduino.h>
#include <time.h>

// Constantes de simulación 
static const unsigned long SIM_PHASE_OPTIMAL_DURATION = 20000UL;
static const unsigned long SIM_PHASE_MODERATE_DURATION = 8000UL;
static const unsigned long SIM_PHASE_CRITICAL_DURATION = 3000UL;
static const unsigned long SIM_CYCLE_DURATION = SIM_PHASE_OPTIMAL_DURATION + 
                                                  SIM_PHASE_MODERATE_DURATION + 
                                                  SIM_PHASE_CRITICAL_DURATION;

// Constructor con struct de pines
ClairDevice::ClairDevice(const ClairPins& pins, 
                         unsigned long scd41Interval,
                         unsigned long pmsInterval,
                         unsigned long reportInterval,
                         int displaySda,
                         int displayScl)
    : scd41Device(pins.scd41_sda, pins.scd41_scl, scd41Interval),
      pms5003Device(pins.pms_rx, pins.pms_tx, pins.pms_set, pins.pms_reset, pmsInterval),
      display(128, 64, displaySda, displayScl, 0x3C, this),
      warningLed(pins.led_pin, false, true, this),  // LED simple: pin, initialState=false, activeHigh=true
      lastReportTime(0),
      reportInterval(reportInterval),
      allSensorsReady(false),
      simulationEnabled(false),
      simulationStartTime(0),
      lastDisplayedStatus(OPTIMAL),
      displayInitialized(false),
      initState(INIT_NOT_STARTED),
      initStartTime(0),
      initTimeoutOccurred(false),
      timeSynchronized(false),
      lastNTPSync(0),
      ntpSyncInterval(3600000),
      timezoneOffset(-18000),
      standbyMode(false) {}

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
      warningLed(ledPin, false, true, this),  // LED simple
      lastReportTime(0),
      reportInterval(reportInterval),
      allSensorsReady(false),
      simulationEnabled(false),
      simulationStartTime(0),
      lastDisplayedStatus(OPTIMAL),
      displayInitialized(false),
      initState(INIT_NOT_STARTED),
      initStartTime(0),
      initTimeoutOccurred(false),
      timeSynchronized(false),
      lastNTPSync(0),
      ntpSyncInterval(3600000),
      timezoneOffset(-18000),
      standbyMode(false) {}

// Inicializar sistema
bool ClairDevice::begin() {
    initState = INIT_STARTING_SENSORS;
    initStartTime = millis();
    initTimeoutOccurred = false;
    allSensorsReady = false;
    
    Serial.println("[ClairDevice] Starting non-blocking initialization...");
    
    scd41Device.getSensor().begin();
    pms5003Device.getSensor().begin();
    display.begin();
    warningLed.off();  // Forzar LED apagado
    
    initState = INIT_WAITING_SENSORS;
    
    return true;
}

// Callback estático para procesar comandos remotos
bool ClairDevice::processRemoteCommand(const RemoteCommand& cmd) {
    Serial.printf("[ClairDevice] Command received: ID=%s, TYPE=%s\n", 
                  cmd.commandId.c_str(), cmd.type.c_str());
    
    extern ClairDevice* g_clairDevice;
    
    if (cmd.type == "STANDBY") {
        Serial.println("[ClairDevice] → Executing STANDBY command");
        if (g_clairDevice) {
            g_clairDevice->setStandbyMode(true);
        }
        return true;
    }
    else if (cmd.type == "WAKE") {
        Serial.println("[ClairDevice] → Executing WAKE command");
        if (g_clairDevice) {
            g_clairDevice->setStandbyMode(false);
        }
        return true;
    }
    else if (cmd.type == "RESTART") {
        Serial.println("[ClairDevice] → Executing RESTART command");
        // TODO: Implementar restart
        return true;
    }
    else if (cmd.type == "REPORT") {
        Serial.println("[ClairDevice] → Executing REPORT command");
        if (g_clairDevice) {
            g_clairDevice->forceReport();
        }
        return true;
    }
    else if (cmd.type == "CALIBRATE") {
        Serial.println("[ClairDevice] → Executing CALIBRATE command");
        // TODO: Implementar calibración
        return true;
    }
    else {
        Serial.printf("[ClairDevice] → Unknown command type: %s\n", cmd.type.c_str());
        return false;
    }
}

// Configurar Incident Manager
void ClairDevice::setupIncidentManager(const String& baseUrl, const String& hardwareId, 
                                        const String& apiKey, unsigned long pollInterval) {
    incidentManager.begin(baseUrl, hardwareId, apiKey, pollInterval);
    incidentManager.setCallbacks(onIncidentDetected, onIncidentResolved);
    Serial.println("[ClairDevice] Incident Manager configured");
}

// Callbacks de incidentes
void ClairDevice::onIncidentDetected(const Incident& incident) {
    Serial.println("\n🔴🔴🔴 INCIDENT ACTIVATED 🔴🔴🔴");
    incident.print();
    // El LED se actualizará automáticamente en updateWarningLed()
}

void ClairDevice::onIncidentResolved(const Incident& incident) {
    Serial.println("\n🟢🟢🟢 INCIDENT RESOLVED 🟢🟢🟢");
    incident.print();
    // El LED se actualizará automáticamente en updateWarningLed()
}

// Control del LED basado en incidentes
void ClairDevice::updateWarningLed() {
    if (standbyMode) {
        warningLed.stopBlink();
        return;
    }
    
    // Comportamiento: LED parpadea si hay incidentes activos
    if (incidentManager.hasActiveIncidents()) {
        if (!warningLed.isBlinking()) {
            warningLed.startBlink(500);  // Parpadeo cada 500ms
            Serial.printf("[LED] 🔴 Blinking started - %d incident(s) active\n", 
                          incidentManager.getActiveCount());
        }
    } else {
        if (warningLed.isBlinking() || warningLed.getState()) {
            warningLed.off();
            Serial.println("[LED] ⚫ OFF - No incidents");
        }
    }
    
    // Necesario para actualizar el estado de parpadeo
    warningLed.update();
}

// Inicializar NTP
void ClairDevice::beginNTP(const char* ntpServer, long timezoneOffset) {
    this->timezoneOffset = timezoneOffset;
    
    configTime(timezoneOffset, 0, ntpServer);
    
    Serial.print("[NTP] Synchronizing time with ");
    Serial.print(ntpServer);
    Serial.println();
    
    unsigned long start = millis();
    while (time(nullptr) < 100000 && (millis() - start < 10000)) {
        delay(100);
    }
    
    if (time(nullptr) >= 100000) {
        timeSynchronized = true;
        lastNTPSync = millis();
        Serial.println("[NTP] Time synchronized successfully");
        
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        Serial.printf("[NTP] Current time: %02d:%02d:%02d\r\n", 
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);        
    } else {
        Serial.println("[NTP] Time synchronization failed - will retry later");
        timeSynchronized = false;
    }
}

void ClairDevice::updateNTP() {
    if (!timeSynchronized && wifi.isConnected()) {
        unsigned long now = millis();
        if (now - lastNTPSync > 30000) {
            lastNTPSync = now;
            configTime(timezoneOffset, 0, "pool.ntp.org");
            
            unsigned long start = millis();
            while (time(nullptr) < 100000 && (millis() - start < 5000)) {
                delay(50);
            }
            
            if (time(nullptr) >= 100000) {
                timeSynchronized = true;
                Serial.println("[NTP] Time synchronized (retry)");
            }
        }
    }
    
    if (timeSynchronized && wifi.isConnected()) {
        if (millis() - lastNTPSync > ntpSyncInterval) {
            lastNTPSync = millis();
            configTime(timezoneOffset, 0, "pool.ntp.org");
            Serial.println("[NTP] Resynchronizing time...");
        }
    }
}

unsigned long ClairDevice::getCurrentEpoch() {
    if (timeSynchronized) {
        return time(nullptr);
    }
    return 0;
}

String ClairDevice::getFormattedTime() {
    if (!timeSynchronized) {
        return "00:00:00";
    }
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "00:00:00";
    }
    
    char buffer[9];
    sprintf(buffer, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return String(buffer);
}

// Actualizar inicialización no bloqueante
void ClairDevice::updateInitialization() {
    if (initState == INIT_COMPLETE || initState == INIT_PARTIAL) {
        return;
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
            initTimeoutOccurred = true;
            
            if (scd41Ready || pmsReady) {
                initState = INIT_PARTIAL;
                allSensorsReady = false;
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
    updateInitialization();
    
    if (initState == INIT_COMPLETE || initState == INIT_PARTIAL) {
        
        unsigned long now = millis();
        
        // === MODO STANDBY ===
        if (standbyMode) {
            wifi.update();
            
            if (wifi.isConnected()) {
                edge.pollCommands();
                edge.processCommandQueue();                
            }
            
            delay(100);
            return;
        }
        
        // === MODO NORMAL ===
        
        if (simulationEnabled) {
            updateSimulationData();
        } else {
            scd41Device.update();
            pms5003Device.update();
            updateAirQualityData();
            updateParticulateMatterData();
        }        
        
        if (!displayInitialized || currentData.status != lastDisplayedStatus) {
            refreshDisplay();
            lastDisplayedStatus = currentData.status;
            displayInitialized = true;
        }
        display.autoPowerManagement();

        updateWarningLed();  // Control del LED
        
        wifi.update();
        updateNTP();
        
        if (wifi.isConnected()) {
            edge.sendTelemetry(currentData);
            edge.pollCommands();        
            edge.processCommandQueue();
            incidentManager.pollIncidents();
            incidentManager.process();
        }
        
        if (now - lastReportTime >= reportInterval) {
            generateUnifiedReport();
            lastReportTime = now;
        }
    }
}

void ClairDevice::printEdgeStats() {
    edge.printStats();
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
    
    currentData.evaluateStatus(thresholds);

    if (timeSynchronized) {
        currentData.timeFormatted = getFormattedTime();
        
        unsigned long uptimeSec = millis() / 1000;
        unsigned long hours = uptimeSec / 3600;
        unsigned long minutes = (uptimeSec % 3600) / 60;
        unsigned long seconds = uptimeSec % 60;
        
        char uptimeBuf[9];
        sprintf(uptimeBuf, "%02d:%02d:%02d", hours, minutes, seconds);
        currentData.uptimeFormatted = String(uptimeBuf);
    }
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
    
    currentData.evaluateStatus(thresholds);
}

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
    // Comandos del sistema
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
    // Comandos para LED simple
    else if (command.id == LED_ON_COMMAND) {
        warningLed.on();
        Serial.println("[LED] Manual ON");
    }
    else if (command.id == LED_OFF_COMMAND) {
        warningLed.off();
        Serial.println("[LED] Manual OFF");
    }
    else if (command.id == LED_BLINK_COMMAND) {
        warningLed.startBlink(500);
        Serial.println("[LED] Manual BLINK started");
    }
    else if (command.id == LED_ACKNOWLEDGE_ALL) {
        warningLed.stopBlink();
        Serial.println("[LED] Blink stopped after acknowledge");
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

void ClairDevice::setSimulationEnabled(bool enabled) {
    simulationEnabled = enabled;
    simulationStartTime = millis();
    displayInitialized = false;
}

void ClairDevice::setStandbyMode(bool standby) {
    if (standbyMode == standby) return;
    
    standbyMode = standby;
    
    if (standbyMode) {
        Serial.println("[ClairDevice] Entering STANDBY mode - suspending all non-essential operations");
        
        display.off();
        warningLed.off();
        
        if (pms5003Device.getSensor().isInitialized()) {
            pms5003Device.getSensor().sleep();
        }
        
        edge.setTelemetryEnabled(false);
        
    } else {
        Serial.println("[ClairDevice] Exiting STANDBY mode - resuming normal operation");
        
        display.on();
        
        if (pms5003Device.getSensor().isInitialized()) {
            pms5003Device.getSensor().wake();
        }
        
        edge.setTelemetryEnabled(true);
        forceReport();
    }
}