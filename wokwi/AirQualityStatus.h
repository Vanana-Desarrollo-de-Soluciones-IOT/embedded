#ifndef AIR_QUALITY_STATUS_H
#define AIR_QUALITY_STATUS_H

#include <Arduino.h>

/**
 * @brief Air Quality status levels
 */
enum AirQualityStatus {
    OPTIMAL,
    MODERATE,
    CRITICAL
};

/**
 * @brief Thresholds for air quality evaluation
 */
struct AirQualityThresholds {
    // PM2.5 thresholds (ug/m3)
    uint16_t pm25ModerateLimit = 35;   // EPA 24-hour standard
    uint16_t pm25CriticalLimit = 55;    // Unhealthy for sensitive groups
    
    // PM10 thresholds (ug/m3)
    uint16_t pm10ModerateLimit = 75;    // EPA 24-hour standard
    uint16_t pm10CriticalLimit = 150;   // Unhealthy
    
    // CO2 thresholds (ppm)
    uint16_t co2ModerateLimit = 1000;   // ASHRAE recommendation
    uint16_t co2CriticalLimit = 1500;   // Poor air quality
    
    // Humidity thresholds (%)
    uint8_t humidityModerateLow = 30;    // Below 30% is too dry
    uint8_t humidityModerateHigh = 70;   // Above 70% promotes mold
    uint8_t humidityCriticalLow = 20;    // Very dry
    uint8_t humidityCriticalHigh = 80;   // Very humid
    
    // Default constructor
    AirQualityThresholds() = default;
    
    // Constructor for custom configuration
    AirQualityThresholds(uint16_t pm25Mod, uint16_t pm25Crit,
                         uint16_t pm10Mod, uint16_t pm10Crit,
                         uint16_t co2Mod, uint16_t co2Crit,
                         uint8_t humModLow, uint8_t humModHigh,
                         uint8_t humCritLow, uint8_t humCritHigh)
        : pm25ModerateLimit(pm25Mod), pm25CriticalLimit(pm25Crit),
          pm10ModerateLimit(pm10Mod), pm10CriticalLimit(pm10Crit),
          co2ModerateLimit(co2Mod), co2CriticalLimit(co2Crit),
          humidityModerateLow(humModLow), humidityModerateHigh(humModHigh),
          humidityCriticalLow(humCritLow), humidityCriticalHigh(humCritHigh) {}
};

/**
 * @brief Converts AirQualityStatus enum to human-readable string
 */
inline const char* airQualityLabel(AirQualityStatus status) {
    switch (status) {
        case CRITICAL:
            return "Critical";
        case MODERATE:
            return "Moderate";
        case OPTIMAL:
        default:
            return "Optimal";
    }
}

/**
 * @brief Converts AirQualityStatus to icon
 */
inline const char* airQualityIcon(AirQualityStatus status) {
    switch (status) {
        case CRITICAL:
            return "Critical";
        case MODERATE:
            return "Moderate";
        case OPTIMAL:
        default:
            return "Optimal";
    }
}

#endif // AIR_QUALITY_STATUS_H
