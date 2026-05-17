#include "PMS5003SensorDevice.h"
#include <Arduino.h>

// Constructor
PMS5003SensorDevice::PMS5003SensorDevice(int rxPin, int txPin, int setPin, 
                                         int resetPin, unsigned long readInterval)
    : sensor(rxPin, txPin, setPin, resetPin, readInterval, this) {}

// Handle events from sensor
void PMS5003SensorDevice::on(Event event) {
    if (event.id == PMS5003Sensor::DATA_READY_EVENT_ID) {
        // New data available - could publish to MQTT, store, etc.
        PMS5003Data data = sensor.getData();
        //Serial.println("[Device] Nuevos datos del sensor PMS5003");
        //Serial.printf("  → PM1.0: %d μg/m³\n", data.pm1_0);
        //Serial.printf("  → PM2.5: %d μg/m³\n", data.pm2_5);
        //Serial.printf("  → PM10:  %d μg/m³\n", data.pm10);
    }
    else if (event.id == PMS5003Sensor::SLEEP_MODE_EVENT_ID) {
        Serial.println("[Device] Sensor en modo sleep");
    }
    else if (event.id == PMS5003Sensor::WAKE_MODE_EVENT_ID) {
        Serial.println("[Device] Sensor despertado");
    }
}

// Handle commands
void PMS5003SensorDevice::handle(Command command) {
    if (command.id == PMS5003Sensor::SLEEP_COMMAND_ID) {
        Serial.println("[Device] Comando: Poner sensor en sleep");
        sensor.sleep();
    }
    else if (command.id == PMS5003Sensor::WAKE_COMMAND_ID) {
        Serial.println("[Device] Comando: Despertar sensor");
        sensor.wake();
    }
    else if (command.id == PMS5003Sensor::RESET_COMMAND_ID) {
        Serial.println("[Device] Comando: Resetear sensor");
        sensor.reset();
    }
}

// Get sensor reference
PMS5003Sensor& PMS5003SensorDevice::getSensor() {
    return sensor;
}

// Update device
void PMS5003SensorDevice::update() {
    sensor.update();
}

// Process serial commands (optional utility)
void PMS5003SensorDevice::processSerialCommand(char command) {
    switch (command) {
        case 's':
        case 'S':
            handle(Command(PMS5003Sensor::SLEEP_COMMAND_ID));
            break;
        case 'w':
        case 'W':
            handle(Command(PMS5003Sensor::WAKE_COMMAND_ID));
            break;
        case 'r':
        case 'R':
            handle(Command(PMS5003Sensor::RESET_COMMAND_ID));
            break;
        default:
            Serial.printf("[Device] Comando desconocido: %c\n", command);
            break;
    }
}