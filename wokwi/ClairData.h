#ifndef CLAIR_DATA_H
#define CLAIR_DATA_H

#include <Arduino.h>
#include "AirQualityStatus.h"

/**
 * @brief Unified data structure for Clair System
 */
struct ClairData {
    // Timestamp
    unsigned long timestamp;
    
    // SCD41 Data
    struct {
        uint16_t co2;
        float temperature;
        float humidity;
        bool valid;
    } airQuality;
    
    // PMS5003 Data
    struct {
        uint16_t pm1_0;
        uint16_t pm2_5;
        uint16_t pm10;
        bool valid;
    } particulateMatter;
    
    // AQI calculations
    struct {
        int aqi;
        String category;
    } airQualityIndex;
    
    // Air Quality Status
    AirQualityStatus status;
    String statusLabel;
    String statusIcon;
    
    // Constructor
    ClairData() {
        timestamp = 0;
        airQuality.co2 = 0;
        airQuality.temperature = 0;
        airQuality.humidity = 0;
        airQuality.valid = false;
        
        particulateMatter.pm1_0 = 0;
        particulateMatter.pm2_5 = 0;
        particulateMatter.pm10 = 0;
        particulateMatter.valid = false;
        
        airQualityIndex.aqi = 0;
        airQualityIndex.category = "Unknown";
        
        status = OPTIMAL;
        statusLabel = "Optimal";
        statusIcon = "Optimal";
    }
    
    // Calculate AQI based on PM2.5
    void calculateAQI() {
        if (!particulateMatter.valid) {
            airQualityIndex.aqi = -1;
            airQualityIndex.category = "No Data";
            return;
        }
        
        if (particulateMatter.pm2_5 <= 12) {
            airQualityIndex.aqi = map(particulateMatter.pm2_5, 0, 12, 0, 50);
            airQualityIndex.category = "Good";
        } 
        else if (particulateMatter.pm2_5 <= 35) {
            airQualityIndex.aqi = map(particulateMatter.pm2_5, 13, 35, 51, 100);
            airQualityIndex.category = "Moderate";
        }
        else if (particulateMatter.pm2_5 <= 55) {
            airQualityIndex.aqi = map(particulateMatter.pm2_5, 36, 55, 101, 150);
            airQualityIndex.category = "Unhealthy for Sensitive";
        }
        else if (particulateMatter.pm2_5 <= 150) {
            airQualityIndex.aqi = map(particulateMatter.pm2_5, 56, 150, 151, 200);
            airQualityIndex.category = "Unhealthy";
        }
        else if (particulateMatter.pm2_5 <= 250) {
            airQualityIndex.aqi = map(particulateMatter.pm2_5, 151, 250, 201, 300);
            airQualityIndex.category = "Very Unhealthy";
        }
        else {
            airQualityIndex.aqi = map(particulateMatter.pm2_5, 251, 500, 301, 500);
            airQualityIndex.category = "Hazardous";
        }
    }
    
    // Evaluate overall air quality status
    void evaluateStatus(const AirQualityThresholds& thresholds = AirQualityThresholds()) {
        // Check for CRITICAL conditions
        if ((particulateMatter.valid && 
             (particulateMatter.pm2_5 >= thresholds.pm25CriticalLimit || 
              particulateMatter.pm10 >= thresholds.pm10CriticalLimit)) ||
            (airQuality.valid && 
             (airQuality.co2 >= thresholds.co2CriticalLimit ||
              airQuality.humidity < thresholds.humidityCriticalLow || 
              airQuality.humidity > thresholds.humidityCriticalHigh))) {
            status = CRITICAL;
            statusLabel = "Critical";
            statusIcon = "Critical";
            return;
        }
        
        // Check for MODERATE conditions
        if ((particulateMatter.valid && 
             (particulateMatter.pm2_5 >= thresholds.pm25ModerateLimit || 
              particulateMatter.pm10 >= thresholds.pm10ModerateLimit)) ||
            (airQuality.valid && 
             (airQuality.co2 >= thresholds.co2ModerateLimit ||
              airQuality.humidity < thresholds.humidityModerateLow || 
              airQuality.humidity > thresholds.humidityModerateHigh))) {
            status = MODERATE;
            statusLabel = "Moderate";
            statusIcon = "Moderate";
            return;
        }
        
        // Default: OPTIMAL
        status = OPTIMAL;
        statusLabel = "Optimal";
        statusIcon = "Optimal";
    }
    
    // Print all data to Serial
    void print() {
        Serial.printf("Time: %lu ms\n", timestamp);
        Serial.printf("Status: %s\n", statusLabel.c_str());
        if (airQuality.valid) {
            Serial.printf("CO2: %4d ppm\n", airQuality.co2);
            Serial.printf("Temperature: %4.1f C\n", airQuality.temperature);
            Serial.printf("Humidity: %4.1f %%\n", airQuality.humidity);
        }

        if (particulateMatter.valid) {
            Serial.printf("PM1.0: %4d ug/m3\n", particulateMatter.pm1_0);
            Serial.printf("PM2.5: %4d ug/m3\n", particulateMatter.pm2_5);
            Serial.printf("PM10:  %4d ug/m3\n", particulateMatter.pm10);
        }

        if (airQualityIndex.aqi >= 0) {
            Serial.printf("AQI: %3d\n", airQualityIndex.aqi);
            Serial.printf("Category: %s\n", airQualityIndex.category.c_str());
        }
    }
};

#endif // CLAIR_DATA_H
