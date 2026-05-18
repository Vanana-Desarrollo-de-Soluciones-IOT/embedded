#ifndef SCD41_SENSOR_DEVICE_H
#define SCD41_SENSOR_DEVICE_H

#include "Device.h"
#include "SCD41Sensor.h"

class SCD41SensorDevice : public Device {
private:
    SCD41Sensor sensor;

public:
    static const int SDA_PIN = 21;
    static const int SCL_PIN = 22;
    static const unsigned long READ_INTERVAL_MS = 2000;

    SCD41SensorDevice(int sdaPin = SDA_PIN, int sclPin = SCL_PIN, unsigned long readInterval = READ_INTERVAL_MS);
    
    void on(Event event) override;
    void handle(Command command) override;
    
    SCD41Sensor& getSensor();
    void update();  // Update the sensor periodically
};

#endif
