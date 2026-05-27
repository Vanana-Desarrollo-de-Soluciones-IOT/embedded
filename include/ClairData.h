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

    // NUEVO: Tiempo real formateado
    String timeFormatted;    // "HH:MM:SS" para la medición
    String uptimeFormatted;   // "HH:MM:SS" para el uptime
    
    String country;  // País desde donde se envía
    
    
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

        timeFormatted = "";
        uptimeFormatted = "";
        country = "PERU";
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
    Serial.println();
    
    // Usar print() + println() para control exacto
    Serial.print("Time:     ");
    Serial.print(timestamp);
    Serial.println(" ms");
    
    //Serial.print("Status:   ");
    //Serial.println(statusLabel.c_str());
    
    if (airQuality.valid) {
        Serial.print("CO2:      ");
        Serial.print(airQuality.co2);
        Serial.println(" ppm");
        
        Serial.print("Temp:     ");
        Serial.print(airQuality.temperature, 1);
        Serial.println(" C");
        
        Serial.print("Humidity: ");
        Serial.print(airQuality.humidity, 1);
        Serial.println(" %");
    }

    if (particulateMatter.valid) {
        Serial.print("PM1.0:    ");
        Serial.print(particulateMatter.pm1_0);
        Serial.println(" ug/m3");
        
        Serial.print("PM2.5:    ");
        Serial.print(particulateMatter.pm2_5);
        Serial.println(" ug/m3");
        
        Serial.print("PM10:     ");
        Serial.print(particulateMatter.pm10);
        Serial.println(" ug/m3");
    }

    if (airQualityIndex.aqi >= 0) {
        Serial.print("AQI:      ");
        Serial.println(airQualityIndex.aqi);
        
        //Serial.print("Category: ");
        //Serial.println(airQualityIndex.category.c_str());
    }
    Serial.println();
}
};

#endif // CLAIR_DATA_H
