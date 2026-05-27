#ifndef RGB_LED_H
#define RGB_LED_H

#include "Actuator.h"

class RgbLed : public Actuator {
private:
    int redPin;
    int greenPin;
    int bluePin;
    bool commonAnode;
    bool redState;
    bool greenState;
    bool blueState;
    bool autoManage;  // NUEVO: Control automático vs manual

    void applyPin(int pin, bool on);

public:
    RgbLed(int redPin, int greenPin, int bluePin,
           bool initialRed = false,
           bool initialGreen = false,
           bool initialBlue = false,
           CommandHandler* commandHandler = nullptr,
           bool commonAnode = true,
           bool autoManage = true);  // NUEVO parámetro

    void setAutoManage(bool enable) { autoManage = enable; }
    bool isAutoManage() const { return autoManage; }
    void handle(Command command) override;
    void setColor(bool red, bool green, bool blue);
    void off();
    bool isOn() const;
};

#endif // RGB_LED_H
