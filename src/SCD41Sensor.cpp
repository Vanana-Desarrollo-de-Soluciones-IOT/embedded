#include "SCD41Sensor.h"
#include <Arduino.h>

// IMPORTANT: Constructor must match the .h exactly
// Uses EventHandler*, not CommandHandler*
SCD41Sensor::SCD41Sensor(int sda, int scl, unsigned long interval, EventHandler* handler)
    : Sensor(-1, handler), scd41(&Wire, 0x62), sdaPin(sda), sclPin(scl), 
      readInterval(interval), lastReadTime(0), dataReady(false), sensorInitialized(false) {
    memset(&lastMeasurement, 0, sizeof(lastMeasurement));
}

SCD41Sensor::~SCD41Sensor() {}

bool SCD41Sensor::begin() {
    Wire.begin(sdaPin, sclPin);
    Wire.setClock(100000);
    
    // Inicialización
    scd41.enablePeriodMeasure(SCD4X_STOP_PERIODIC_MEASURE);
    delay(500);
    scd41.moduleReinit();
    delay(50);
    scd41.setAutoCalibMode(true);
    scd41.enablePeriodMeasure(SCD4X_START_PERIODIC_MEASURE);
    
    // Espera con puntos en la misma línea
    Serial.print("SCD41 starting");
    unsigned long startTime = millis();
    
    for (int i = 0; i < 4; i++) {
        delay(1000);
        Serial.print(" .");
    }
    
    unsigned long elapsed = (millis() - startTime) / 1000;
    Serial.printf(" ✓ Ready after %lu seconds\n", elapsed);
    
    sensorInitialized = true;
    return true;
}

void SCD41Sensor::update() {
    if (!sensorInitialized) return;
    
    unsigned long now = millis();
    if (now - lastReadTime >= readInterval) {
        if (scd41.getDataReadyStatus()) {
            scd41.readMeasurement(&lastMeasurement);
            
            if (handler != nullptr) {
                handler->on(Event(DATA_READY_EVENT_ID));
            }
        }
        lastReadTime = now;
    }
}

void SCD41Sensor::on(Event event) {
    if (event.id == DATA_READY_EVENT_ID) {
    }
}

uint16_t SCD41Sensor::getCO2() {
    return lastMeasurement.CO2ppm;
}

float SCD41Sensor::getTemperature() {
    return lastMeasurement.temp;
}

float SCD41Sensor::getHumidity() {
    return lastMeasurement.humidity;
}

void SCD41Sensor::printSerialNumber() {
    uint16_t serialNumber[3];
    if (scd41.getSerialNumber(serialNumber)) {
        Serial.print("Serial number: ");
        Serial.print(serialNumber[0], HEX);
        Serial.print(" ");
        Serial.print(serialNumber[1], HEX);
        Serial.print(" ");
        Serial.println(serialNumber[2], HEX);
    }
}

bool SCD41Sensor::recalibrate(uint16_t targetCO2ppm) {
    int16_t result = scd41.performForcedRecalibration(targetCO2ppm);
    if (result >= 0) {
        return true;
    } else {
        Serial.println("SCD41 recalibration failed");
        return false;
    }
}
