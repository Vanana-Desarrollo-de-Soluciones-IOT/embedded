// ClairDevice.h - corrected version

#ifndef CLAIR_DEVICE_H
#define CLAIR_DEVICE_H

#include "Device.h"
#include "SCD41SensorDevice.h"
#include "PMS5003SensorDevice.h"
#include "OLEDDisplay.h"
#include "ClairData.h"
#include "AirQualityStatus.h"
#include "Led.h" 
#include "WiFiService.h"
#include "CloudService.h"

/**
 * @brief Pin configuration structure for Clair System
 */
struct ClairPins {
    int scd41_sda = 21;
    int scd41_scl = 22;
    int pms_rx = 16;
    int pms_tx = 17;
    int pms_set = 18;
    int pms_reset = 19;
    
    ClairPins() = default;
    ClairPins(int sda, int scl, int rx, int tx, int set, int reset) 
        : scd41_sda(sda), scd41_scl(scl), pms_rx(rx), pms_tx(tx), 
          pms_set(set), pms_reset(reset) {}
};

/**
 * @brief Clair Device - Main environmental monitoring system
 */
class ClairDevice : public Device {
private:
    SCD41SensorDevice scd41Device;
    PMS5003SensorDevice pms5003Device;
    OLEDDisplay display;
    Led warningLed;
        
    ClairData currentData;
    AirQualityThresholds thresholds; 

    WiFiService wifi;
    CloudService cloud;
    unsigned long lastCloudSend;
    
    unsigned long lastReportTime;
    unsigned long reportInterval;
    bool allSensorsReady;

    // LED control
    unsigned long lastLedBlink;
    bool ledBlinkState;
    
    // Private methods
    void updateAirQualityData();
    void updateParticulateMatterData();
    void generateUnifiedReport();
    void checkSensorStatus();
    void updateWarningLed();
    
    // Helper method for display
    String getAirQualityLabel(int co2);  // add this line
    
public:
    // Command IDs for Clair System
    static const int CLAIR_REPORT_COMMAND = 1000;
    static const int CLAIR_CALIBRATE_COMMAND = 1001;
    static const int CLAIR_RESET_COMMAND = 1002;
    
    // Constructor con struct
    ClairDevice(const ClairPins& pins = ClairPins(), 
                unsigned long scd41Interval = 2000,
                unsigned long pmsInterval = 2000,
                unsigned long reportInterval = 10000,
                int displaySda = 21,
                int displayScl = 22,
                int ledPin = 2);
    
    // Constructor with individual parameters
    ClairDevice(int sda, int scl, int rx, int tx, int set, int reset,
                unsigned long scd41Interval = 2000,
                unsigned long pmsInterval = 2000,
                unsigned long reportInterval = 10000,
                int displaySda = 21,
                int displayScl = 22,
                int ledPin = 2);
    
    bool begin();
    void update();
    void on(Event event) override;
    void handle(Command command) override;
    
    ClairData getCurrentData();
    void forceReport();
    bool isSystemReady() const { return allSensorsReady; }
    
    SCD41SensorDevice& getSCD41Device() { return scd41Device; }
    PMS5003SensorDevice& getPMS5003Device() { return pms5003Device; }
    OLEDDisplay& getDisplay() { return display; }
    Led& getWarningLed() { return warningLed; }

    void setAirQualityThresholds(const AirQualityThresholds& newThresholds) {
        thresholds = newThresholds;
    }
    AirQualityStatus getCurrentStatus() const {
        return currentData.status;
    }
    String getCurrentStatusLabel() const {
        return currentData.statusLabel;
    }

    // Cloud services
    void setupWiFi(const String& ssid, const String& password);
    void setupCloud(const String& endpoint, const String& deviceId, unsigned long interval = 30000);
    void setCloudEnabled(bool enabled) { cloud.setEnabled(enabled); }
    bool isCloudEnabled() const { return cloud.isEnabled(); }

     // WiFi status
    bool isWiFiConnected() const { return wifi.isConnected(); }
    String getWiFiIP() const { return wifi.getLocalIP(); }
};

#endif // CLAIR_DEVICE_H
