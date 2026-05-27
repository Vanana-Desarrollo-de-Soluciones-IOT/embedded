#include "OLEDDisplay.h"
#include <Arduino.h>

// Constructor
OLEDDisplay::OLEDDisplay(int width, int height, int sda, int scl, 
                         uint8_t address, CommandHandler* handler)
    : Actuator(-1, handler),
      display(width, height, &Wire, -1),
      sdaPin(sda), sclPin(scl), i2cAddress(address),
      initialized(false), currentState(DISPLAY_OFF),
      lastUpdateTime(0), lastWakeTime(0),
      sleepAfterMs(30000), wakeDurationMs(10000) {}

OLEDDisplay::~OLEDDisplay() {}

// Initialize display
bool OLEDDisplay::begin() {       
    Wire.begin(sdaPin, sclPin);
    Wire.setClock(400000);
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, i2cAddress)) {
        Serial.println("OLED display not found");
        return false;
    }
    
    initialized = true;
    currentState = DISPLAY_ON;
    
    // Eliminar el delay(2000) y mostrar inicio rápido
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Clair System");
    display.println("Starting...");
    display.display();
    
    return true;
}

// Update display with new data
void OLEDDisplay::updateData(const DisplayData& data) {
    currentData = data;
    lastUpdateTime = millis();
    
    // Auto-wake if data arrives and display is sleeping
    if (currentState == DISPLAY_SLEEP) {
        wake();
    }
    
    refresh();
}

// Refresh display
void OLEDDisplay::refresh() {
    if (!initialized) return;
    if (currentState == DISPLAY_OFF) return;
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);  // Smallest text size
    
    int y = 0;
    int lineHeight = 8;  // Reduced from 10 to 8 pixels
    
    // Line 1: Status
    display.setCursor(0, y);
    //display.print("Status: ");
    //display.println(currentData.airQuality.statusLabel);
    y += lineHeight;
    
    // Blank line
    y += lineHeight/2;
    
    // Line 2: PM1.0
    display.setCursor(0, y);
    display.print("PM1.0: ");
    if (currentData.particulateMatter.valid) {
        display.print(currentData.particulateMatter.pm1_0);
    } else {
        display.print("--");
    }
    display.println(" ug/m3");
    y += lineHeight;
    
    // Line 3: PM2.5
    display.setCursor(0, y);
    display.print("PM2.5: ");
    if (currentData.particulateMatter.valid) {
        display.print(currentData.particulateMatter.pm2_5);
    } else {
        display.print("--");
    }
    display.println(" ug/m3");
    y += lineHeight;
    
    // Line 4: PM10
    display.setCursor(0, y);
    display.print("PM10 : ");
    if (currentData.particulateMatter.valid) {
        display.print(currentData.particulateMatter.pm10);
    } else {
        display.print("--");
    }
    display.println(" ug/m3");
    y += lineHeight;
    
    // Line 5: CO2
    display.setCursor(0, y);
    display.print("CO2  : ");
    if (currentData.airQuality.valid) {
        display.print(currentData.airQuality.co2);
    } else {
        display.print("--");
    }
    display.println(" ppm");
    y += lineHeight;
    
    // Line 6: T/H
    display.setCursor(0, y);
    display.print("T/H  : ");
    if (currentData.airQuality.valid) {
        display.print(currentData.airQuality.temperature, 1);
        display.print("C ");
        display.print(currentData.airQuality.humidity, 0);
        display.print("%");
    } else {
        display.print("--");
    }
    
    display.display();
}

// Clear display
void OLEDDisplay::clear() {
    if (!initialized) return;
    display.clearDisplay();
    display.display();
}

// Put display to sleep
void OLEDDisplay::sleep() {
    if (!initialized) return;
    if (currentState == DISPLAY_SLEEP) return;
    
    Serial.println("OLED display entering sleep mode");
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    currentState = DISPLAY_SLEEP;
    lastWakeTime = millis();
}

// Wake display from sleep
void OLEDDisplay::wake() {
    if (!initialized) return;
    if (currentState != DISPLAY_SLEEP) return;
    
    Serial.println("OLED display waking up");
    display.ssd1306_command(SSD1306_DISPLAYON);
    currentState = DISPLAY_ON;
    refresh();  // Redraw content
}

// Turn display off completely
void OLEDDisplay::off() {
    if (!initialized) return;
    
    Serial.println("OLED display turning off");
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    currentState = DISPLAY_OFF;
}

// Turn display on
void OLEDDisplay::on() {
    if (!initialized) return;
    
    Serial.println("OLED display turning on");
    display.ssd1306_command(SSD1306_DISPLAYON);
    currentState = DISPLAY_ON;
    refresh();
}

// Handle commands
void OLEDDisplay::handle(Command command) {
    if (command.id == DISPLAY_ON_COMMAND) {
        on();
    }
    else if (command.id == DISPLAY_OFF_COMMAND) {
        off();
    }
    else if (command.id == DISPLAY_SLEEP_COMMAND) {
        sleep();
    }
    else if (command.id == DISPLAY_WAKE_COMMAND) {
        wake();
    }
    else if (command.id == DISPLAY_CLEAR_COMMAND) {
        clear();
    }
}

// Automatic power management
void OLEDDisplay::autoPowerManagement() {
    if (!initialized) return;
    if (currentState == DISPLAY_OFF) return;
    
    unsigned long now = millis();
    
    // Auto-sleep after inactivity
    if (currentState == DISPLAY_ON && (now - lastUpdateTime) > sleepAfterMs) {
        sleep();
    }
}

// Helper: Get air quality label based on CO2
String OLEDDisplay::getAirQualityLabel(int co2) {
    if (co2 < 400) return "Excellent";
    if (co2 < 600) return "Good";
    if (co2 < 800) return "Normal";
    if (co2 < 1000) return "Moderate";
    if (co2 < 1500) return "Poor";
    return "Bad";
}
