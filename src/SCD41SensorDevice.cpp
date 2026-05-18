#include "SCD41SensorDevice.h"
#include <Arduino.h>

SCD41SensorDevice::SCD41SensorDevice(int sdaPin, int sclPin, unsigned long readInterval)
    : sensor(sdaPin, sclPin, readInterval, this) {}

void SCD41SensorDevice::on(Event event) {
    if (event.id == SCD41Sensor::DATA_READY_EVENT_ID) {
        (void)event;
    }
}

void SCD41SensorDevice::handle(Command command) {
    if (command.id == 300) {
        sensor.recalibrate(400);
    }
}

SCD41Sensor& SCD41SensorDevice::getSensor() {
    return sensor;
}

void SCD41SensorDevice::update() {
    sensor.update();  // Sensor checks and reads automatically when data is ready
}
