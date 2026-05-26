#include "RgbLed.h"
#include <Arduino.h>

RgbLed::RgbLed(int redPin, int greenPin, int bluePin,
               bool initialRed,    // false por defecto
               bool initialGreen,  // false por defecto  
               bool initialBlue,   // false por defecto
               CommandHandler* commandHandler,
               bool commonAnode)   // true por defecto (ANODO COMÚN)
    : Actuator(-1, commandHandler),
      redPin(redPin), greenPin(greenPin), bluePin(bluePin),
      commonAnode(commonAnode),  // true por defecto
      redState(initialRed),      // false
      greenState(initialGreen),  // false
      blueState(initialBlue) {   // false
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    pinMode(bluePin, OUTPUT);
    setColor(initialRed, initialGreen, initialBlue);
}

void RgbLed::applyPin(int pin, bool on) {
    bool level = commonAnode ? !on : on;
    digitalWrite(pin, level ? HIGH : LOW);
}

void RgbLed::handle(Command command) {
    Actuator::handle(command);
}

void RgbLed::setColor(bool red, bool green, bool blue) {
    redState = red;
    greenState = green;
    blueState = blue;
    applyPin(redPin, redState);
    applyPin(greenPin, greenState);
    applyPin(bluePin, blueState);
}

void RgbLed::off() {
    setColor(false, false, false);
}

bool RgbLed::isOn() const {
    return redState || greenState || blueState;
}
