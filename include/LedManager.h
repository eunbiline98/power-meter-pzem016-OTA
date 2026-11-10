#pragma once

#include <Arduino.h>

#include "Config/Config.h"

enum LedMode {
    LED_OFF,
    LED_ON,
    LED_BLINK_SLOW,
    LED_BLINK_FAST,
    LED_PULSE_ONCE
};

class LedManager {
public:
    void begin();
    void loop();

    void setMode(LedMode mode);
    void pulseOnce(uint16_t durationMs = 50);

private:
    LedMode currentMode = LED_OFF;

    unsigned long lastBlink = 0;
    unsigned long pulseStart = 0;

    bool ledState = false;
    bool pulseActive = false;

    uint16_t pulseDuration = 50;
};