#ifndef POWER_METER_H
#define POWER_METER_H

#include <Arduino.h>
#include <ModbusMaster.h>

#include "Config/Config.h"

class PowerMeter {
public:
    struct Data {
        float voltage = 0;
        float current = 0;
        float power = 0;
        float energy = 0;
        float frequency = 0;
        float pf = 0;
        String mcb1 = "UNKNOWN";
        String status = "ERR";
        String rssi = "0";
    };

    void begin();
    bool read(Data &outData);
    void resetEnergy();
    void updateMCBStatus();
    String getMCBStatus() const;

private:
    HardwareSerial &pzemSerial = Serial2;
    ModbusMaster node;
    String stMCB1 = "UNKNOWN";

    static void preTransmission();
    static void postTransmission();
};

#endif