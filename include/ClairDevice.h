// ClairDevice.h - Versión limpia solo con LED simple

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
#include "EdgeService.h"
#include "IncidentManager.h"

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
    int led_pin = 25;       // Pin para el LED simple
    
    ClairPins() = default;
    
    ClairPins(int sda, int scl, int rx, int tx, int set, int reset, int led = 25) 
        : scd41_sda(sda), scd41_scl(scl), pms_rx(rx), pms_tx(tx), 
          pms_set(set), pms_reset(reset), led_pin(led) {}
};

/**
 * @brief Clair Device - Main environmental monitoring system
 */
class ClairDevice : public Device {
private:
    // Estados de inicialización
    enum InitState {
        INIT_NOT_STARTED,
        INIT_STARTING_SENSORS,
        INIT_WAITING_SENSORS,
        INIT_COMPLETE,
        INIT_PARTIAL
    };
    
    InitState initState;
    unsigned long initStartTime;
    static const unsigned long INIT_TIMEOUT_MS = 10000;
    bool initTimeoutOccurred;
    
    // Sensores y actuadores
    SCD41SensorDevice scd41Device;
    PMS5003SensorDevice pms5003Device;
    OLEDDisplay display;
    Led warningLed;              // LED simple
    
    // Datos y configuración
    ClairData currentData;
    AirQualityThresholds thresholds;
    
    // Servicios
    WiFiService wifi;
    EdgeService edge;
    IncidentManager incidentManager;
    
    // Control de tiempo
    unsigned long lastReportTime;
    unsigned long reportInterval;
    bool allSensorsReady;
    
    // Simulación
    bool simulationEnabled;
    unsigned long simulationStartTime;
    
    // Display
    AirQualityStatus lastDisplayedStatus;
    bool displayInitialized;
    
    // Modo standby
    bool standbyMode;
    
    // NTP y tiempo real
    bool timeSynchronized;
    unsigned long lastNTPSync;
    unsigned long ntpSyncInterval;
    long timezoneOffset;
    
    // Métodos privados
    void updateAirQualityData();
    void updateParticulateMatterData();
    void generateUnifiedReport();
    void checkSensorStatus();
    void refreshDisplay();
    void updateSimulationData();
    void updateInitialization();
    void updateWarningLed();     // Control del LED basado en incidentes
    
    // Callbacks estáticos
    static void onIncidentDetected(const Incident& incident);
    static void onIncidentResolved(const Incident& incident);
    static bool processRemoteCommand(const RemoteCommand& cmd);
    
    // Helper methods
    String getAirQualityLabel(int co2);
    
public:
    // Command IDs for Clair System
    static const int CLAIR_REPORT_COMMAND = 1000;
    static const int CLAIR_CALIBRATE_COMMAND = 1001;
    static const int CLAIR_RESET_COMMAND = 1002;
    
    // Comandos remotos
    static const int REMOTE_STANDBY_COMMAND = 2000;
    static const int REMOTE_WAKE_COMMAND = 2001;
    static const int REMOTE_RESTART_COMMAND = 2002;
    static const int REMOTE_SET_THRESHOLDS_COMMAND = 2003;
    
    // Comandos para LED
    static const int LED_ON_COMMAND = 3000;
    static const int LED_OFF_COMMAND = 3001;
    static const int LED_BLINK_COMMAND = 3002;
    static const int LED_ACKNOWLEDGE_ALL = 3003;
    
    // Constructor con struct de pines
    ClairDevice(const ClairPins& pins = ClairPins(), 
                unsigned long scd41Interval = 2000,
                unsigned long pmsInterval = 2000,
                unsigned long reportInterval = 10000,
                int displaySda = 21,
                int displayScl = 22);
    
    // Constructor con parámetros individuales
    ClairDevice(int sda, int scl, int rx, int tx, int set, int reset,
                unsigned long scd41Interval = 2000,
                unsigned long pmsInterval = 2000,
                unsigned long reportInterval = 10000,
                int displaySda = 21,
                int displayScl = 22,
                int ledPin = 25);
    
    // Métodos principales
    bool begin();
    void update();
    void on(Event event) override;
    void handle(Command command) override;
    
    // Datos
    ClairData getCurrentData();
    void forceReport();
    bool isSystemReady() const { return allSensorsReady; }
    
    // Acceso a componentes
    SCD41SensorDevice& getSCD41Device() { return scd41Device; }
    PMS5003SensorDevice& getPMS5003Device() { return pms5003Device; }
    OLEDDisplay& getDisplay() { return display; }
    Led& getWarningLed() { return warningLed; }
    
    // Configuración de umbrales
    void setAirQualityThresholds(const AirQualityThresholds& newThresholds) {
        thresholds = newThresholds;
    }
    
    // Estado actual
    AirQualityStatus getCurrentStatus() const {
        return currentData.status;
    }
    
    String getCurrentStatusLabel() const {
        return currentData.statusLabel;
    }
    
    // Edge Service
    void setupEdge(const String& baseUrl, const String& hardwareId, const String& deviceSecret,
                   unsigned long telemetryInterval = 15000,
                   unsigned long commandPollInterval = 10000) {
        edge.begin(baseUrl, hardwareId, deviceSecret, telemetryInterval, commandPollInterval);
        edge.setCommandCallback(processRemoteCommand);
    }
    
    // Incident Manager
    void setupIncidentManager(const String& baseUrl, const String& hardwareId, 
                              const String& apiKey, unsigned long pollInterval = 5000);
    
    bool hasActiveIncidents() const { return incidentManager.hasActiveIncidents(); }
    
    // Modo standby
    bool isStandbyMode() const { return standbyMode; }
    void setStandbyMode(bool standby);
    
    // Estadísticas
    void printEdgeStats();
    
    // WiFi
    void setupWiFi(const String& ssid, const String& password);
    bool isWiFiConnected() const { return wifi.isConnected(); }
    String getWiFiIP() const { return wifi.getLocalIP(); }
    
    // Simulación
    void setSimulationEnabled(bool enabled);
    bool isSimulationEnabled() const { return simulationEnabled; }
    
    // Inicialización
    bool isInitializationComplete() const { 
        return initState == INIT_COMPLETE || initState == INIT_PARTIAL; 
    }
    
    const char* getInitStateString() const;
    
    // NTP y tiempo real
    void beginNTP(const char* ntpServer = "pool.ntp.org", long timezoneOffset = -18000);
    void updateNTP();
    bool isTimeSynchronized() const { return timeSynchronized; }
    String getFormattedTime();
    unsigned long getCurrentEpoch();
};

#endif // CLAIR_DEVICE_H