/**
 * @file Led.cpp
 * @brief Implements the Led class.
 *
 * Manages LED state changes in the Modest IoT Nano-framework, responding to predefined commands
 * (toggle, on, off, blink) and updating the hardware pin accordingly.
 *
 * @author Angel Velasquez
 * @date March 22, 2025
 * @version 0.2
 */

/*
 * This file is part of the Modest IoT Nano-framework (C++ Edition).
 * Copyright (c) 2025 Angel Velasquez
 *
 * Licensed under the Creative Commons Attribution-NoDerivatives 4.0 International (CC BY-ND 4.0).
 * You may use, copy, and distribute this software in its original, unmodified form, provided
 * you give appropriate credit to the original author (Angel Velasquez) and include this notice.
 * Modifications, adaptations, or derivative works are not permitted.
 *
 * Full license text: https://creativecommons.org/licenses/by-nd/4.0/legalcode
 */

#include "Led.h"
#include <Arduino.h>

const Command Led::TOGGLE_LED_COMMAND = Command(TOGGLE_LED_COMMAND_ID);
const Command Led::TURN_ON_COMMAND = Command(TURN_ON_COMMAND_ID);
const Command Led::TURN_OFF_COMMAND = Command(TURN_OFF_COMMAND_ID);
const Command Led::START_BLINK_COMMAND = Command(START_BLINK_COMMAND_ID);
const Command Led::STOP_BLINK_COMMAND = Command(STOP_BLINK_COMMAND_ID);

Led::Led(int pin, bool initialState, bool activeHigh, CommandHandler* commandHandler)
    : Actuator(pin, commandHandler), 
      state(initialState), 
      blinking(false), 
      lastBlinkTime(0), 
      blinkInterval(500), 
      blinkState(initialState),
      activeHigh(activeHigh) {
    pinMode(pin, OUTPUT);
    applyState();
}

void Led::applyState() {
    if (activeHigh) {
        digitalWrite(pin, state ? HIGH : LOW);
    } else {
        digitalWrite(pin, state ? LOW : HIGH);
    }
}

void Led::applyBlinkState() {
    if (activeHigh) {
        digitalWrite(pin, blinkState ? HIGH : LOW);
    } else {
        digitalWrite(pin, blinkState ? LOW : HIGH);
    }
}

void Led::handle(Command command) {
    if (command == TOGGLE_LED_COMMAND) {
        toggle();
    } else if (command == TURN_ON_COMMAND) {
        on();
    } else if (command == TURN_OFF_COMMAND) {
        off();
    } else if (command == START_BLINK_COMMAND) {
        startBlink();
    } else if (command == STOP_BLINK_COMMAND) {
        stopBlink();
    }
    Actuator::handle(command); // Propagate to handler if set
}

bool Led::getState() const {
    return state;
}

void Led::setState(bool newState) {
    state = newState;
    if (!blinking) {
        applyState();
    }
}

void Led::on() {
    blinking = false;
    state = true;
    applyState();
}

void Led::off() {
    blinking = false;
    state = false;
    applyState();
}

void Led::toggle() {
    blinking = false;
    state = !state;
    applyState();
}

void Led::startBlink(unsigned long intervalMs) {
    blinking = true;
    blinkInterval = intervalMs;
    lastBlinkTime = millis();
    blinkState = true;  // Start with ON
    applyBlinkState();
}

void Led::stopBlink() {
    blinking = false;
    off();  // Apagar al detener el parpadeo
}

void Led::update() {
    if (!blinking) return;
    
    unsigned long now = millis();
    if (now - lastBlinkTime >= blinkInterval) {
        lastBlinkTime = now;
        blinkState = !blinkState;
        applyBlinkState();
    }
}