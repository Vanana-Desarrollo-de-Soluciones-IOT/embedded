#ifndef LED_H
#define LED_H

/**
 * @file Led.h
 * @brief Declares the Led class.
 *
 * A concrete actuator class in the Modest IoT Nano-framework for controlling an LED. 
 * Supports toggle, on, off commands, and blinking functionality.
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

#include "Actuator.h"

class Led : public Actuator {
private:
    bool state;           ///< Current state of the LED (true = ON, false = OFF).
    bool blinking;        ///< Whether the LED is in blinking mode.
    unsigned long lastBlinkTime;  ///< Last time the blink state changed.
    unsigned long blinkInterval;  ///< Blink interval in milliseconds.
    bool blinkState;      ///< Current blink cycle state.
    bool activeHigh;      ///< true = HIGH turns on, false = LOW turns on.

public:
    // Command IDs
    static const int TOGGLE_LED_COMMAND_ID = 0;    ///< Toggle LED.
    static const int TURN_ON_COMMAND_ID = 1;       ///< Turn LED ON.
    static const int TURN_OFF_COMMAND_ID = 2;      ///< Turn LED OFF.
    static const int START_BLINK_COMMAND_ID = 3;   ///< Start blinking.
    static const int STOP_BLINK_COMMAND_ID = 4;    ///< Stop blinking.
    
    static const Command TOGGLE_LED_COMMAND;
    static const Command TURN_ON_COMMAND;
    static const Command TURN_OFF_COMMAND;
    static const Command START_BLINK_COMMAND;
    static const Command STOP_BLINK_COMMAND;

    /**
     * @brief Constructs an Led actuator.
     * @param pin The GPIO pin for the LED (configured as OUTPUT).
     * @param initialState Initial state of the LED (default: false/OFF).
     * @param activeHigh true = HIGH turns on, false = LOW turns on (default: true).
     * @param commandHandler Optional handler to receive commands (default: nullptr).
     */
    Led(int pin, bool initialState = false, bool activeHigh = true, CommandHandler* commandHandler = nullptr);

    /**
     * @brief Handles commands to control the LED state.
     * @param command The command to execute.
     */
    void handle(Command command) override;

    /**
     * @brief Gets the current state of the LED.
     * @return True if the LED is ON, false if OFF.
     */
    bool getState() const;

    /**
     * @brief Sets the LED state directly.
     * @param newState The new state (true = ON, false = OFF).
     */
    void setState(bool newState);
    
    /**
     * @brief Turns LED on (cancels blinking).
     */
    void on();
    
    /**
     * @brief Turns LED off (cancels blinking).
     */
    void off();
    
    /**
     * @brief Toggles LED state (cancels blinking).
     */
    void toggle();
    
    /**
     * @brief Starts blinking with specified interval.
     * @param intervalMs Blink interval in milliseconds (default: 500ms).
     */
    void startBlink(unsigned long intervalMs = 500);
    
    /**
     * @brief Stops blinking and turns LED off.
     */
    void stopBlink();
    
    /**
     * @brief Updates blink state - call this periodically in loop().
     */
    void update();
    
    /**
     * @brief Checks if LED is currently blinking.
     * @return true if blinking, false otherwise.
     */
    bool isBlinking() const { return blinking; }
    
    /**
     * @brief Sets blink interval.
     * @param intervalMs New blink interval in milliseconds.
     */
    void setBlinkInterval(unsigned long intervalMs) { blinkInterval = intervalMs; }

private:
    void applyState();
    void applyBlinkState();  // <--- AÑADIR ESTA DECLARACIÓN
};

#endif // LED_H