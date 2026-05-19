// ClairDevice.h - corrected version

#ifndef CLAIR_DEVICE_H
#define CLAIR_DEVICE_H

#include "Device.h"
#include "SCD41SensorDevice.h"
#include "PMS5003SensorDevice.h"
#include "OLEDDisplay.h"
#include "ClairData.h"
#include "AirQualityStatus.h"
#include "RgbLed.h" 
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
    int rgb_red = 27;
    int rgb_green = 26;
    int rgb_blue = 25;
    
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

    enum InitState {
        INIT_NOT_STARTED,
        INIT_STARTING_SENSORS,
        INIT_WAITING_SENSORS,
        INIT_COMPLETE,
        INIT_PARTIAL
    };
    InitState initState;
    unsigned long initStartTime;
    static const unsigned long INIT_TIMEOUT_MS = 10000;  // 10 segundos máximo
    bool initTimeoutOccurred;

    SCD41SensorDevice scd41Device;
    PMS5003SensorDevice pms5003Device;
    OLEDDisplay display;
    RgbLed warningLed;
        
    ClairData currentData;
    AirQualityThresholds thresholds; 

    WiFiService wifi;
    CloudService cloud;
    unsigned long lastCloudSend;
    
    unsigned long lastReportTime;
    unsigned long reportInterval;
    bool allSensorsReady;
    bool simulationEnabled;
    unsigned long simulationStartTime;

    // RGB status control
    AirQualityStatus lastDisplayedStatus;
    bool displayInitialized;
    
    // Private methods
    void updateAirQualityData();
    void updateParticulateMatterData();
    void generateUnifiedReport();
    void checkSensorStatus();
    void updateWarningLed();
    void refreshDisplay();
    void updateSimulationData();
    void updateInitialization();
    
    // Helper method for display
    String getAirQualityLabel(int co2);  // add this line

    // Simulation helpers
    enum SimulationPhase { PHASE_OPTIMAL, PHASE_MODERATE, PHASE_CRITICAL };
    SimulationPhase getCurrentSimulationPhase(unsigned long elapsed) const;
    void fillOptimalSimulationData(ClairData& data, unsigned long now);
    void fillModerateSimulationData(ClairData& data, unsigned long now);
    void fillCriticalSimulationData(ClairData& data, unsigned long now);

    bool timeSynchronized;
    unsigned long lastNTPSync;
    unsigned long ntpSyncInterval;  // Resincronizar cada hora
    long timezoneOffset;  // Diferencia en segundos respecto a UTC
    
public:
    // Command IDs for Clair System
    static const int CLAIR_REPORT_COMMAND = 1000;
    static const int CLAIR_CALIBRATE_COMMAND = 1001;
    static const int CLAIR_RESET_COMMAND = 1002;
    
    // Constructor with pin struct
    ClairDevice(const ClairPins& pins = ClairPins(), 
                unsigned long scd41Interval = 2000,
                unsigned long pmsInterval = 2000,
                unsigned long reportInterval = 10000,
                int displaySda = 21,
                int displayScl = 22);
    
    // Constructor with individual parameters
    ClairDevice(int sda, int scl, int rx, int tx, int set, int reset,
                unsigned long scd41Interval = 2000,
                unsigned long pmsInterval = 2000,
                unsigned long reportInterval = 10000,
                int displaySda = 21,
                int displayScl = 22,
                int rgbRedPin = 25,
                int rgbGreenPin = 26,
                int rgbBluePin = 27);
    
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
    RgbLed& getWarningLed() { return warningLed; }

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
    void setupCloud(const String& endpoint, const String& hardwareId, const String& deviceSecret, unsigned long interval);
    void setCloudEnabled(bool enabled) { cloud.setEnabled(enabled); }
    bool isCloudEnabled() const { return cloud.isEnabled(); }

    void setSimulationEnabled(bool enabled);
    bool isSimulationEnabled() const { return simulationEnabled; }

     // WiFi status
    bool isWiFiConnected() const { return wifi.isConnected(); }
    String getWiFiIP() const { return wifi.getLocalIP(); }
    
    // NUEVO: Verificar si la inicialización está completa
    bool isInitializationComplete() const { 
        return initState == INIT_COMPLETE || initState == INIT_PARTIAL; 
    }
    
    // NUEVO: Obtener estado de inicialización
    const char* getInitStateString() const;

    // NUEVO: Configurar NTP para tiempo real    
    void beginNTP(const char* ntpServer = "pool.ntp.org", long timezoneOffset = -18000);  // -18000 = UTC-5 (Colombia/Perú/New York)
    void updateNTP();
    bool isTimeSynchronized() const { return timeSynchronized; }
    String getFormattedTime();  // Retorna "HH:MM:SS"
    unsigned long getCurrentEpoch();  // Retorna segundos desde 1970
};

#endif // CLAIR_DEVICE_H
