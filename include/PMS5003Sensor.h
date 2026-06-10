#ifndef PMS5003_SENSOR_H
#define PMS5003_SENSOR_H

#include "Sensor.h"
#include <HardwareSerial.h>

/**
 * @brief Structure to hold particulate matter readings
 */
struct PMS5003Data {
    uint16_t pm1_0;   // PM1.0 concentration (ug/m3)
    uint16_t pm2_5;   // PM2.5 concentration (ug/m3)
    uint16_t pm10;    // PM10 concentration (ug/m3)
    bool valid;       // Flag indicating if data is valid
};

/**
 * @brief PMS5003 Sensor class following ModestIoT architecture
 * 
 * Implements the Plantower PMS5003 particulate matter sensor
 * using UART communication.
 */
class PMS5003Sensor : public Sensor {
private:
    // Pin definitions
    int rxPin;
    int txPin;
    int setPin;     // Control sleep mode (HIGH = normal, LOW = sleep)
    int resetPin;   // Reset module (pulse LOW)
    
    // Communication
    HardwareSerial serial;
    unsigned long readInterval;
    unsigned long lastReadTime;
    
    // State
    bool sensorInitialized;
    bool dataReady;
    bool isSleeping;
    
    // Data
    PMS5003Data lastData;
    
    // Frame buffer for reading
    static const int FRAME_SIZE = 32;
    uint8_t buffer[FRAME_SIZE];
    
    // Private methods
    bool readFrame(PMS5003Data& data);
    uint16_t calculateChecksum(uint8_t* buffer, int length);
    
public:
    // Event IDs for communication with Device
    static const int DATA_READY_EVENT_ID = 100;
    static const int SLEEP_MODE_EVENT_ID = 101;
    static const int WAKE_MODE_EVENT_ID = 102;
    
    // Command IDs
    static const int SLEEP_COMMAND_ID = 200;
    static const int WAKE_COMMAND_ID = 201;
    static const int RESET_COMMAND_ID = 202;
    
    /**
     * @brief Constructor for PMS5003 sensor
     * @param rxPin UART RX pin (connect to PMS TX)
     * @param txPin UART TX pin (connect to PMS RX)
     * @param setPin Control pin for sleep mode
     * @param resetPin Reset pin
     * @param interval Read interval in milliseconds
     * @param handler Event handler (typically the Device)
     */
    PMS5003Sensor(int rxPin, int txPin, int setPin, int resetPin, 
                  unsigned long interval, EventHandler* handler = nullptr);
    ~PMS5003Sensor();
    
    /**
     * @brief Initialize the sensor
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
    
    /**
     * @brief Get latest sensor data
     */
    PMS5003Data getData();
    
    /**
     * @brief Put sensor in sleep mode (low power)
     */
    void sleep();
    
    /**
     * @brief Wake sensor from sleep mode
     */
    void wake();
    
    /**
     * @brief Reset the sensor module
     */
    void reset();
    
    /**
     * @brief Check if sensor is initialized
     */
    bool isInitialized() const { return sensorInitialized; }
    
    /**
     * @brief Check if sensor is in sleep mode
     */
    bool isSleepingMode() const { return isSleeping; }
};

#endif // PMS5003_SENSOR_H
