#ifndef PMS5003_SENSOR_DEVICE_H
#define PMS5003_SENSOR_DEVICE_H

#include "Device.h"
#include "PMS5003Sensor.h"

/**
 * @brief PMS5003 Sensor Device following ModestIoT architecture
 * 
 * Combines the PMS5003 sensor with device-level event and command handling.
 */
class PMS5003SensorDevice : public Device {
private:
    PMS5003Sensor sensor;
    
public:
    // Default pin configuration
    static const int DEFAULT_RX_PIN = 16;
    static const int DEFAULT_TX_PIN = 17;
    static const int DEFAULT_SET_PIN = 18;
    static const int DEFAULT_RESET_PIN = 19;
    static const unsigned long DEFAULT_READ_INTERVAL_MS = 2000;
    
    /**
     * @brief Constructor with configurable pins
     */
    PMS5003SensorDevice(int rxPin = DEFAULT_RX_PIN, 
                        int txPin = DEFAULT_TX_PIN,
                        int setPin = DEFAULT_SET_PIN,
                        int resetPin = DEFAULT_RESET_PIN,
                        unsigned long readInterval = DEFAULT_READ_INTERVAL_MS);
    
    /**
     * @brief Handle events from sensor
     */
    void on(Event event) override;
    
    /**
     * @brief Handle commands (e.g., from Serial, MQTT)
     */
    void handle(Command command) override;
    
    /**
     * @brief Get access to sensor instance
     */
    PMS5003Sensor& getSensor();
    
    /**
     * @brief Update device state - call periodically
     */
    void update();
    
    /**
     * @brief Process serial commands (optional)
     */
    void processSerialCommand(char command);
};

#endif // PMS5003_SENSOR_DEVICE_H