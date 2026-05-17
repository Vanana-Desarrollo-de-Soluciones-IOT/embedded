#include "SCD41Sensor.h"
#include <Arduino.h>

// IMPORTANTE: El constructor debe coincidir EXACTAMENTE con el .h
// Usa EventHandler*, NO CommandHandler*
SCD41Sensor::SCD41Sensor(int sda, int scl, unsigned long interval, EventHandler* handler)
    : Sensor(-1, handler), scd41(&Wire, 0x62), sdaPin(sda), sclPin(scl), 
      readInterval(interval), lastReadTime(0), dataReady(false), sensorInitialized(false) {
    memset(&lastMeasurement, 0, sizeof(lastMeasurement));
}

SCD41Sensor::~SCD41Sensor() {}

bool SCD41Sensor::begin() {
    Wire.begin(sdaPin, sclPin);
    Wire.setClock(100000);
    
    Serial.println("Inicializando sensor SCD41...");
    
    if (!scd41.begin()) {
        Serial.println("Error: No se pudo inicializar el sensor SCD41");
        return false;
    }
    
    sensorInitialized = true;
    Serial.println("Sensor SCD41 inicializado correctamente!");
    
    printSerialNumber();
    
    scd41.setAutoCalibMode(true);
    Serial.print("Modo calibración automática: ");
    Serial.println(scd41.getAutoCalibMode() ? "Activado" : "Desactivado");
    
    scd41.enablePeriodMeasure(SCD4X_START_PERIODIC_MEASURE);
    Serial.println("Mediciones periódicas iniciadas (cada 5 segundos)");
    
    delay(6000);
    
    return true;
}

void SCD41Sensor::update() {
    if (!sensorInitialized) return;
    
    unsigned long now = millis();
    if (now - lastReadTime >= readInterval) {
        if (scd41.getDataReadyStatus()) {
            scd41.readMeasurement(&lastMeasurement);
            
            //Serial.println("=================================");
            //Serial.print("CO2: ");
            //Serial.print(lastMeasurement.CO2ppm);
            //Serial.println(" ppm");
            //Serial.print("Temperatura: ");
            //Serial.print(lastMeasurement.temp, 1);
            //Serial.println(" °C");
            //Serial.print("Humedad: ");
            //Serial.print(lastMeasurement.humidity, 1);
            //Serial.println(" %");
            //Serial.println("=================================\n");
            
            // Notificar al Device (que es EventHandler) que hay nuevos datos
            if (handler != nullptr) {
                handler->on(Event(DATA_READY_EVENT_ID));
            }
        }
        lastReadTime = now;
    }
}

void SCD41Sensor::on(Event event) {
    // El sensor puede responder a eventos si es necesario
    if (event.id == DATA_READY_EVENT_ID) {
        // Los datos ya fueron leídos en update()
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
        Serial.print("Número de serie: ");
        Serial.print(serialNumber[0], HEX);
        Serial.print(" ");
        Serial.print(serialNumber[1], HEX);
        Serial.print(" ");
        Serial.println(serialNumber[2], HEX);
    } else {
        Serial.println("Error al leer número de serie");
    }
}

bool SCD41Sensor::recalibrate(uint16_t targetCO2ppm) {
    int16_t result = scd41.performForcedRecalibration(targetCO2ppm);
    if (result >= 0) {
        Serial.printf("Recalibración exitosa. Corrección: %d ppm\n", result);
        return true;
    } else {
        Serial.println("Error en recalibración");
        return false;
    }
}