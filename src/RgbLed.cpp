#include "RgbLed.h"
#include <Arduino.h>

RgbLed::RgbLed(int redPin, int greenPin, int bluePin,
               bool initialRed, bool initialGreen, bool initialBlue,
               CommandHandler* commandHandler, bool commonAnode,
               bool autoManage)
    : Actuator(-1, commandHandler),
      redPin(redPin), greenPin(greenPin), bluePin(bluePin),
      commonAnode(commonAnode),
      redState(initialRed), greenState(initialGreen), blueState(initialBlue),
      autoManage(autoManage) {
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    pinMode(bluePin, OUTPUT);

     // Solo aplicar si NO está en modo automático
    if (!autoManage) {
    setColor(initialRed, initialGreen, initialBlue);  // Aplica colores iniciales
    } else {
    off();  // Solo si autoManage = true
    }
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
