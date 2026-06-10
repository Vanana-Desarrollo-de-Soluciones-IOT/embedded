#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "Actuator.h"
#include "CommandHandler.h"
#include <Wire.h>
#include <Adafruit_SSD1306.h>

/**
 * @brief Display states for power management
 */
enum DisplayState {
    DISPLAY_ON,
    DISPLAY_SLEEP,
    DISPLAY_OFF
};

/**
 * @brief Data structure for display information
 */
struct DisplayData {
    struct AirQuality {
        int co2;
        float temperature;
        float humidity;
        bool valid;
        String statusLabel;
    } airQuality;
    
    struct ParticulateMatter {
        uint16_t pm1_0;
        uint16_t pm2_5;
        uint16_t pm10;
        bool valid;
    } particulateMatter;
    
    struct AQI {
        int value;
        String category;
    } aqi;
    
    DisplayData() {
        airQuality.co2 = 0;
        airQuality.temperature = 0;
        airQuality.humidity = 0;
        airQuality.valid = false;
        airQuality.statusLabel = "Unknown";
        
        particulateMatter.pm1_0 = 0;
        particulateMatter.pm2_5 = 0;
        particulateMatter.pm10 = 0;
        particulateMatter.valid = false;
        
        aqi.value = 0;
        aqi.category = "Unknown";
    }
};

/**
 * @brief OLED Display Actuator for Clair System
 * 
 * Implements a 128x64 OLED display (SSD1306) following ModestIoT architecture.
 * Supports power management and command-based control.
 */
class OLEDDisplay : public Actuator {
private:
    // Hardware
    Adafruit_SSD1306 display;
    int sdaPin;
    int sclPin;
    uint8_t i2cAddress;
    
    // State
    bool initialized;
    DisplayState currentState;
    unsigned long lastUpdateTime;
    unsigned long lastWakeTime;
    DisplayData currentData;
    
    // Power management
    unsigned long sleepAfterMs;
    unsigned long wakeDurationMs;
    
    // Display buffer (for partial updates)
    char lineBuffer[32];
    
    // Private methods
    void drawStatusBar();
    void drawAirQualityData();
    void drawParticulateMatterData();
    void drawAQI();
    void drawFooter();
    String getAirQualityLabel(int co2);
    
public:
    // Event IDs
    static const int DISPLAY_UPDATE_EVENT = 300;
    static const int DISPLAY_WAKE_EVENT = 301;
    static const int DISPLAY_SLEEP_EVENT = 302;
    
    // Command IDs
    static const int DISPLAY_ON_COMMAND = 400;
    static const int DISPLAY_OFF_COMMAND = 401;
    static const int DISPLAY_SLEEP_COMMAND = 402;
    static const int DISPLAY_WAKE_COMMAND = 403;
    static const int DISPLAY_CLEAR_COMMAND = 404;
    
    /**
     * @brief Constructor for OLED Display
     * @param width Display width (default 128)
     * @param height Display height (default 64)
     * @param sda I2C SDA pin
     * @param scl I2C SCL pin
     * @param address I2C address (default 0x3C)
     * @param handler Command handler (typically the Device)
     */
    OLEDDisplay(int width = 128, int height = 64, 
                int sda = 21, int scl = 22, 
                uint8_t address = 0x3C,
                CommandHandler* handler = nullptr);
    
    ~OLEDDisplay();
    
    /**
     * @brief Initialize the display
     * @return true if successful
     */
    bool begin();
    
    /**
     * @brief Update display with new data
     * @param data New display data
     */
    void updateData(const DisplayData& data);
    
    /**
     * @brief Refresh display (redraw all)
     */
    void refresh();
    
    /**
     * @brief Clear display
     */
    void clear();
    
    /**
     * @brief Put display to sleep (power saving)
     */
    void sleep();
    
    /**
     * @brief Wake display from sleep
     */
    void wake();
    
    /**
     * @brief Turn display off completely
     */
    void off();
    
    /**
     * @brief Turn display on
     */
    void on();
    
    /**
     * @brief Handle commands from Device
     */
    void handle(Command command) override;
    
    /**
     * @brief Automatic power management - call periodically
     */
    void autoPowerManagement();
    
    /**
     * @brief Check if display is initialized
     */
    bool isInitialized() const { return initialized; }
    
    /**
     * @brief Get current display state
     */
    DisplayState getState() const { return currentState; }
    
    /**
     * @brief Set auto-sleep timeout
     * @param ms Milliseconds of inactivity before sleep
     */
    void setSleepTimeout(unsigned long ms) { sleepAfterMs = ms; }
};

#endif // OLED_DISPLAY_H