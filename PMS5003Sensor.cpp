#include "PMS5003Sensor.h"
#include <Arduino.h>

// Constructor
PMS5003Sensor::PMS5003Sensor(int rxPin, int txPin, int setPin, int resetPin, 
                             unsigned long interval, EventHandler* handler)
    : Sensor(-1, handler), 
      rxPin(rxPin), txPin(txPin), setPin(setPin), resetPin(resetPin),
      readInterval(interval), lastReadTime(0),
      sensorInitialized(false), dataReady(false), isSleeping(false) {
    
    // Initialize data structure
    memset(&lastData, 0, sizeof(lastData));
    memset(buffer, 0, FRAME_SIZE);
    
    // Create serial port
    serial = new HardwareSerial(2);  // Use UART2
}

// Destructor
PMS5003Sensor::~PMS5003Sensor() {
    delete serial;
}

// Initialize sensor
bool PMS5003Sensor::begin() {
    Serial.println("Inicializando sensor PMS5003...");
    
    // Configure control pins
    pinMode(setPin, OUTPUT);
    pinMode(resetPin, OUTPUT);
    
    // Initial state: normal mode (not sleeping)
    digitalWrite(setPin, HIGH);
    digitalWrite(resetPin, HIGH);
    isSleeping = false;
    
    // Perform soft reset
    delay(100);
    reset();
    
    // Initialize UART communication
    serial->begin(9600, SERIAL_8N1, rxPin, txPin);
    
    sensorInitialized = true;
    Serial.println("Sensor PMS5003 inicializado correctamente!");
    Serial.println("  → UART2 configurado a 9600 baudios");
    Serial.println("  → Modo normal (activo)");
    
    return true;
}

// Update sensor - called periodically
void PMS5003Sensor::update() {
    if (!sensorInitialized) return;
    
    unsigned long now = millis();
    if (now - lastReadTime >= readInterval) {
        PMS5003Data newData;
        
        if (readFrame(newData) && newData.valid) {
            lastData = newData;
            dataReady = true;
            
            // Print readings 
            //Serial.println("╔════════════════════════════════════════╗");
            //Serial.println("║         PMS5003 Particulate Matter     ║");
            //Serial.println("╠════════════════════════════════════════╣");
            //Serial.printf("║ PM1.0: %4d μg/m³                      ║\n", lastData.pm1_0);
            //Serial.printf("║ PM2.5: %4d μg/m³                      ║\n", lastData.pm2_5);
            //Serial.printf("║ PM10:  %4d μg/m³                      ║\n", lastData.pm10);
            //Serial.println("╚════════════════════════════════════════╝");
            
            // Notify Device that new data is available
            if (handler != nullptr) {
                handler->on(Event(DATA_READY_EVENT_ID));
            }
            
            dataReady = false;
        }
        
        lastReadTime = now;
    }
}

// Read a complete frame from PMS5003
bool PMS5003Sensor::readFrame(PMS5003Data& data) {
    if (serial->available() < FRAME_SIZE) {
        data.valid = false;
        return false;
    }
    
    while (serial->available() >= FRAME_SIZE) {
        // Look for start byte (0x42)
        if (serial->peek() != 0x42) {
            serial->read();  // Discard byte
            continue;
        }
        
        // Read potential frame
        serial->readBytes(buffer, FRAME_SIZE);
        
        // Verify frame header
        if (buffer[0] != 0x42 || buffer[1] != 0x4D) {
            continue;  // Invalid header
        }
        
        // Verify checksum
        uint16_t calculatedChecksum = calculateChecksum(buffer, FRAME_SIZE - 2);
        uint16_t receivedChecksum = ((uint16_t)buffer[30] << 8) | buffer[31];
        
        if (calculatedChecksum != receivedChecksum) {
            continue;  // Checksum mismatch
        }
        
        // Extract data (big-endian format)
        data.pm1_0 = ((uint16_t)buffer[4] << 8) | buffer[5];
        data.pm2_5 = ((uint16_t)buffer[6] << 8) | buffer[7];
        data.pm10  = ((uint16_t)buffer[8] << 8) | buffer[9];
        data.valid = true;
        
        return true;
    }
    
    data.valid = false;
    return false;
}

// Calculate checksum for PMS5003 frame
uint16_t PMS5003Sensor::calculateChecksum(uint8_t* buffer, int length) {
    uint16_t sum = 0;
    for (int i = 0; i < length; i++) {
        sum += buffer[i];
    }
    return sum;
}

// Handle events from Device
void PMS5003Sensor::on(Event event) {
    if (event.id == DATA_READY_EVENT_ID) {
        // Data already read in update(), just acknowledge
        if (handler != nullptr) {
            handler->on(Event(DATA_READY_EVENT_ID));
        }
    }
}

// Get latest data
PMS5003Data PMS5003Sensor::getData() {
    return lastData;
}

// Put sensor to sleep
void PMS5003Sensor::sleep() {
    if (!sensorInitialized) return;
    
    Serial.println("PMS5003: Entrando en modo sleep...");
    digitalWrite(setPin, LOW);
    isSleeping = true;
    
    if (handler != nullptr) {
        handler->on(Event(SLEEP_MODE_EVENT_ID));
    }
}

// Wake sensor from sleep
void PMS5003Sensor::wake() {
    if (!sensorInitialized) return;
    
    Serial.println("PMS5003: Despertando...");
    digitalWrite(setPin, HIGH);
    isSleeping = false;
    delay(100);  // Allow sensor to stabilize
    
    if (handler != nullptr) {
        handler->on(Event(WAKE_MODE_EVENT_ID));
    }
}

// Reset sensor module
void PMS5003Sensor::reset() {
    if (!sensorInitialized) return;
    
    Serial.println("PMS5003: Reseteando módulo...");
    digitalWrite(resetPin, LOW);
    delay(10);
    digitalWrite(resetPin, HIGH);
    delay(100);  // Wait for sensor to restart
    
    // ELIMINA estas líneas:
    // if (handler != nullptr) {
    //     handler->handle(Command(RESET_COMMAND_ID));
    // }
    
    // En su lugar, solo notifica el evento (si quieres)
    if (handler != nullptr) {
        handler->on(Event(RESET_COMMAND_ID));  // Usa on(), no handle()
    }
}