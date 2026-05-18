#include "PMS5003SensorDevice.h"
#include <Arduino.h>

// Constructor
PMS5003SensorDevice::PMS5003SensorDevice(int rxPin, int txPin, int setPin, 
                                         int resetPin, unsigned long readInterval)
    : sensor(rxPin, txPin, setPin, resetPin, readInterval, this) {}

// Handle events from sensor
void PMS5003SensorDevice::on(Event event) {
    if (event.id == PMS5003Sensor::DATA_READY_EVENT_ID) {
        (void)sensor.getData();
    }
}

// Handle commands
void PMS5003SensorDevice::handle(Command command) {
    if (command.id == PMS5003Sensor::SLEEP_COMMAND_ID) {
        sensor.sleep();
    }
    else if (command.id == PMS5003Sensor::WAKE_COMMAND_ID) {
        sensor.wake();
    }
    else if (command.id == PMS5003Sensor::RESET_COMMAND_ID) {
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
            break;
    }
}
