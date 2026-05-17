#ifndef SCD41_SENSOR_H
#define SCD41_SENSOR_H

#include "Sensor.h"
#include "DFRobot_SCD4X.h"
#include <Wire.h>

class SCD41Sensor : public Sensor {
private:
    class MySCD41 : public DFRobot_SCD4X {
    public:
        MySCD41(TwoWire *pWire = &Wire, uint8_t i2cAddr = 0x62) 
            : DFRobot_SCD4X(pWire, i2cAddr) {}
        
        bool getSerialNumber(uint16_t *wordBuf) {
            return DFRobot_SCD4X::getSerialNumber(wordBuf);
        }
        
        int16_t performForcedRecalibration(uint16_t CO2ppm) {
            return DFRobot_SCD4X::performForcedRecalibration(CO2ppm);
        }
    };
    
    MySCD41 scd41;
    int sdaPin;
    int sclPin;
    unsigned long readInterval;
    unsigned long lastReadTime;
    bool dataReady;
    bool sensorInitialized;
    
    DFRobot_SCD4X::sSensorMeasurement_t lastMeasurement;
    
public:
    static const int DATA_READY_EVENT_ID = 100;
    
    // IMPORTANTE: El handler debe ser EventHandler*, NO CommandHandler*
    SCD41Sensor(int sda, int scl, unsigned long interval, EventHandler* handler = nullptr);
    ~SCD41Sensor();
    
    bool begin();
    void update();
    void on(Event event) override;
    
    uint16_t getCO2();
    float getTemperature();
    float getHumidity();
    void printSerialNumber();
    bool recalibrate(uint16_t targetCO2ppm);
    
    bool isDataReady() const { return dataReady; }
    bool isInitialized() const { return sensorInitialized; }
};

#endif