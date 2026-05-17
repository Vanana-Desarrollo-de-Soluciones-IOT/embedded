#include "SCD41SensorDevice.h"
#include <Arduino.h>

SCD41SensorDevice::SCD41SensorDevice(int sdaPin, int sclPin, unsigned long readInterval)
    : sensor(sdaPin, sclPin, readInterval, this) {}

void SCD41SensorDevice::on(Event event) {
    if (event.id == SCD41Sensor::DATA_READY_EVENT_ID) {
        // Los datos ya fueron leídos y mostrados por el sensor
        // Aquí podrías enviar los datos por MQTT, Serial, etc.
        //Serial.println("Device: Nuevos datos del sensor disponibles");
        
        // Ejemplo: acceder a los datos a través del sensor
        //Serial.print("Device - Último CO2: ");
        //Serial.print(sensor.getCO2());
        //Serial.println(" ppm");
    }
}

void SCD41SensorDevice::handle(Command command) {
    // Manejar comandos externos (ej: desde Serial, MQTT)
    if (command.id == 300) {
        Serial.println("Device: Comando de recalibración recibido");
        sensor.recalibrate(400);
    }
}

SCD41Sensor& SCD41SensorDevice::getSensor() {
    return sensor;
}

void SCD41SensorDevice::update() {
    sensor.update();  // El sensor verifica y lee automáticamente cuando hay datos
}