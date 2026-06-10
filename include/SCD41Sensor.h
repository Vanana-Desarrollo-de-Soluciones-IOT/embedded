#ifndef SCD41_SENSOR_H
#define SCD41_SENSOR_H

#include "Sensor.h"
#include <Wire.h>
#include <SensirionI2cScd4x.h>

/**
 * @brief SCD41 Sensor class using Sensirion I2C library
 * 
 * Implements the Sensirion SCD41 CO2, temperature, and humidity sensor
 * with proper initialization (wakeUp, reinit, startPeriodicMeasurement).
 */
class SCD41Sensor : public Sensor {
private:
    SensirionI2cScd4x scd4x;
    int sdaPin;
    int sclPin;
    unsigned long readInterval;
    unsigned long lastReadTime;
    bool sensorInitialized;
    bool dataValid;
    
    // Last readings
    uint16_t lastCO2;
    float lastTemperature;
    float lastHumidity;
    
    // Error handling
    char errorMessage[64];
    
public:
    // Event ID for data ready
    static const int DATA_READY_EVENT_ID = 100;
    
    /**
     * @brief Constructor for SCD41 sensor
     * @param sda I2C SDA pin
     * @param scl I2C SCL pin
     * @param interval Read interval in milliseconds
     * @param handler Event handler (typically the Device)
     */
    SCD41Sensor(int sda, int scl, unsigned long interval, EventHandler* handler = nullptr);
    ~SCD41Sensor();
    
    /**
     * @brief Initialize the sensor with full wake-up sequence
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * @brief Update sensor state - call periodically
     */
    void update();
    
    /**
     * @brief Handle events from Device
     */
    void on(Event event) override;
    
    // Getters
    uint16_t getCO2() const { return lastCO2; }
    float getTemperature() const { return lastTemperature; }
    float getHumidity() const { return lastHumidity; }
    bool isInitialized() const { return sensorInitialized; }
    bool isDataValid() const { return dataValid; }
    
    // Utility methods
    void printSerialNumber();
    bool recalibrate(uint16_t targetCO2ppm);
    void performForcedRecalibration(uint16_t targetCO2ppm);
    
private:
    /**
     * @brief Read measurement from sensor
     * @return true if reading was successful and valid
     */
    bool readMeasurement();
};

#endif // SCD41_SENSOR_H