#include "SCD41Sensor.h"
#include <Arduino.h>

// Definir NO_ERROR si no está definido por la librería
#ifndef NO_ERROR
#define NO_ERROR 0
#endif

SCD41Sensor::SCD41Sensor(int sda, int scl, unsigned long interval, EventHandler* handler)
    : Sensor(-1, handler), 
      sdaPin(sda), 
      sclPin(scl), 
      readInterval(interval), 
      lastReadTime(0),
      sensorInitialized(false),
      dataValid(false),
      lastCO2(0),
      lastTemperature(0.0f),
      lastHumidity(0.0f) {
    memset(errorMessage, 0, sizeof(errorMessage));
}

SCD41Sensor::~SCD41Sensor() {}

bool SCD41Sensor::begin() {
    Serial.println("[SCD41] Initializing sensor...");
    
    // Inicializar I2C
    Wire.begin(sdaPin, sclPin);
    Wire.setClock(100000);
    
    // Inicializar el sensor con su dirección I2C
    scd4x.begin(Wire, SCD41_I2C_ADDR_62);
    
    // PASO 1: Despertar el sensor (el SCD41 comienza en sleep)
    int16_t error = scd4x.wakeUp();
    if (error != NO_ERROR) {
        errorToString(error, errorMessage, sizeof(errorMessage));
        Serial.printf("[SCD41] wakeUp() error: %s\n", errorMessage);
        Serial.println("[SCD41] Check connections: VDD->3.3V, GND->GND, SDA->GPIO21, SCL->GPIO22");
        return false;
    }
    Serial.println("[SCD41] ✓ Sensor woken up");
    
    // PASO 2: Detener mediciones periódicas si las hubiera
    error = scd4x.stopPeriodicMeasurement();
    if (error != NO_ERROR) {
        errorToString(error, errorMessage, sizeof(errorMessage));
        Serial.printf("[SCD41] stopPeriodicMeasurement() warning: %s\n", errorMessage);
        // Continuar de todas formas
    }
    
    // PASO 3: Reinicializar el sensor
    error = scd4x.reinit();
    if (error != NO_ERROR) {
        errorToString(error, errorMessage, sizeof(errorMessage));
        Serial.printf("[SCD41] reinit() error: %s\n", errorMessage);
        return false;
    }
    Serial.println("[SCD41] ✓ Sensor reinitialized");
    
    // PASO 4: Leer número de serie para verificar comunicación completa
    uint64_t serialNumber = 0;
    error = scd4x.getSerialNumber(serialNumber);
    if (error != NO_ERROR) {
        errorToString(error, errorMessage, sizeof(errorMessage));
        Serial.printf("[SCD41] getSerialNumber() error: %s\n", errorMessage);
        // No es crítico, continuar
    } else {
        Serial.print("[SCD41] ✓ Serial number: 0x");
        Serial.print((uint32_t)(serialNumber >> 32), HEX);
        Serial.print((uint32_t)(serialNumber & 0xFFFFFFFF), HEX);
        Serial.println();
    }
    
    // PASO 5: Iniciar mediciones periódicas
    error = scd4x.startPeriodicMeasurement();
    if (error != NO_ERROR) {
        errorToString(error, errorMessage, sizeof(errorMessage));
        Serial.printf("[SCD41] startPeriodicMeasurement() error: %s\n", errorMessage);
        return false;
    }
    Serial.println("[SCD41] ✓ Periodic measurements started");
    Serial.println("[SCD41] Note: First reading available after ~5 seconds");
    
    sensorInitialized = true;
    lastReadTime = millis();
    
    return true;
}

bool SCD41Sensor::readMeasurement() {
    if (!sensorInitialized) return false;
    
    // Verificar si hay datos listos
    bool dataReady = false;
    int16_t error = scd4x.getDataReadyStatus(dataReady);
    
    if (error != NO_ERROR) {
        errorToString(error, errorMessage, sizeof(errorMessage));
        Serial.printf("[SCD41] getDataReadyStatus() error: %s\n", errorMessage);
        return false;
    }
    
    if (!dataReady) {
        // Datos no listos aún
        return false;
    }
    
    // Leer la medición
    uint16_t co2 = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    
    error = scd4x.readMeasurement(co2, temperature, humidity);
    
    if (error != NO_ERROR) {
        errorToString(error, errorMessage, sizeof(errorMessage));
        Serial.printf("[SCD41] readMeasurement() error: %s\n", errorMessage);
        return false;
    }
    
    // Validar que la lectura sea razonable
    if (co2 == 0 || co2 > 5000) {
        Serial.printf("[SCD41] Invalid reading: CO2=%d ppm (ignoring)\n", co2);
        return false;
    }
    
    // Almacenar valores válidos
    lastCO2 = co2;
    lastTemperature = temperature;
    lastHumidity = humidity;
    dataValid = true;
    
    Serial.printf("[SCD41] Reading: CO2=%d ppm, T=%.1f°C, H=%.1f%%\n", 
                  co2, temperature, humidity);
    
    return true;
}

void SCD41Sensor::update() {
    if (!sensorInitialized) return;
    
    unsigned long now = millis();
    if (now - lastReadTime >= readInterval) {
        lastReadTime = now;
        
        if (readMeasurement()) {
            // Notificar al Device que hay nuevos datos
            if (handler != nullptr) {
                handler->on(Event(DATA_READY_EVENT_ID));
            }
        }
    }
}

void SCD41Sensor::on(Event event) {
    // Propagate event to handler if needed
    if (handler != nullptr) {
        handler->on(event);
    }
}

void SCD41Sensor::printSerialNumber() {
    if (!sensorInitialized) {
        Serial.println("[SCD41] Sensor not initialized");
        return;
    }
    
    uint64_t serialNumber = 0;
    int16_t error = scd4x.getSerialNumber(serialNumber);
    
    if (error != NO_ERROR) {
        errorToString(error, errorMessage, sizeof(errorMessage));
        Serial.printf("[SCD41] Failed to read serial number: %s\n", errorMessage);
        return;
    }
    
    Serial.print("[SCD41] Serial number: 0x");
    Serial.print((uint32_t)(serialNumber >> 32), HEX);
    Serial.print((uint32_t)(serialNumber & 0xFFFFFFFF), HEX);
    Serial.println();
}

bool SCD41Sensor::recalibrate(uint16_t targetCO2ppm) {
    if (!sensorInitialized) {
        Serial.println("[SCD41] Cannot recalibrate: sensor not initialized");
        return false;
    }
    
    Serial.printf("[SCD41] Performing forced recalibration to %d ppm...\n", targetCO2ppm);
    
    // Nota: La librería SensirionI2cScd4x no tiene performForcedRecalibration directamente
    // Se puede implementar usando performForcedRecalibration si está disponible
    // Por ahora, devolvemos false indicando que no está implementado
    
    Serial.println("[SCD41] Forced recalibration not implemented in this library version");
    Serial.println("[SCD41] Use performForcedRecalibration() method instead");
    return false;
}

void SCD41Sensor::performForcedRecalibration(uint16_t targetCO2ppm) {
    if (!sensorInitialized) {
        Serial.println("[SCD41] Cannot recalibrate: sensor not initialized");
        return;
    }
    
    Serial.printf("[SCD41] Perform forced recalibration to %d ppm...\n", targetCO2ppm);
    
    // Detener mediciones periódicas temporalmente
    scd4x.stopPeriodicMeasurement();
    delay(100);
    
    // Realizar recalibración forzada
    uint16_t correction = 0;
    int16_t error = scd4x.performForcedRecalibration(targetCO2ppm, correction);
    
    if (error == NO_ERROR) {
        Serial.printf("[SCD41] Recalibration successful! Correction: %d ppm\n", correction);
    } else {
        errorToString(error, errorMessage, sizeof(errorMessage));
        Serial.printf("[SCD41] Recalibration failed: %s\n", errorMessage);
    }
    
    // Reiniciar mediciones periódicas
    delay(100);
    scd4x.startPeriodicMeasurement();
}