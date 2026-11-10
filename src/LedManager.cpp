#include "LedManager.h"

void LedManager::begin() {
    pinMode(LED_INDI, OUTPUT);
    digitalWrite(LED_INDI, LOW);
}

void LedManager::setMode(LedMode mode) {
    currentMode = mode;
}

void LedManager::pulseOnce(uint16_t durationMs) {
    pulseDuration = durationMs;
    pulseStart = millis();
    pulseActive = true;
    digitalWrite(LED_INDI, HIGH);
}

void LedManager::loop() {
    const unsigned long now = millis();

    if (pulseActive) {
        if (now - pulseStart >= pulseDuration) {
            pulseActive = false;
            digitalWrite(LED_INDI, LOW);
        }
        return;
    }

    switch (currentMode) {
    case LED_OFF:
        digitalWrite(LED_INDI, LOW);
        break;

    case LED_ON:
        digitalWrite(LED_INDI, HIGH);
        break;

    case LED_BLINK_SLOW:
        if (now - lastBlink >= 1000) {
            lastBlink = now;
            ledState = !ledState;
            digitalWrite(LED_INDI, ledState);
        }
        break;

    case LED_BLINK_FAST:
        if (now - lastBlink >= 250) {
            lastBlink = now;
            ledState = !ledState;
            digitalWrite(LED_INDI, ledState);
        }
        break;

    default:
        break;
    }
}