#include "PowerMeter.h"

#include "LedManager.h"

extern LedManager ledManager;

namespace {

unsigned long startMillis = 0;

} // namespace

void PowerMeter::preTransmission() {
    if (millis() - startMillis > 5000) {
        digitalWrite(MAX485_RE, HIGH);
        digitalWrite(MAX485_DE, HIGH);
        delay(1);
    }
}

void PowerMeter::postTransmission() {
    if (millis() - startMillis > 5000) {
        delay(3);
        digitalWrite(MAX485_RE, LOW);
        digitalWrite(MAX485_DE, LOW);
    }
}

void PowerMeter::begin() {
    startMillis = millis();

    pinMode(MAX485_RE, OUTPUT);
    pinMode(MAX485_DE, OUTPUT);

    digitalWrite(MAX485_RE, LOW);
    digitalWrite(MAX485_DE, LOW);

    pzemSerial.begin(9600, SERIAL_8N2, MAX485_RO, MAX485_DI);

    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    node.begin(PZEM_SLAVE_ADDR, pzemSerial);

    Serial.print("[PZEM] Addr: 0x");
    Serial.println(PZEM_SLAVE_ADDR, HEX);
    Serial.print("[PZEM] RX=");
    Serial.print(MAX485_RO);
    Serial.print(" TX=");
    Serial.println(MAX485_DI);

    ledManager.setMode(LED_BLINK_SLOW);

    while (millis() - startMillis < 5000) {
        ledManager.loop();
        delay(1);
    }

    const uint8_t result = node.readInputRegisters(0x0000, 1);

    if (result == node.ku8MBSuccess) {
        Serial.println("[MODBUS] PZEM-016 connected");
        ledManager.setMode(LED_ON);
    } else {
        Serial.print("[MODBUS] PZEM-016 failed, error: 0x");
        Serial.println(result, HEX);
        ledManager.setMode(LED_BLINK_FAST);
    }

    Serial.println("[PZEM] Modbus ready");
}

void PowerMeter::updateMCBStatus() {
    const int state = digitalRead(MCB_1);
    if (state == HIGH) {
        stMCB1 = "OFF";
    } else {
        stMCB1 = "ON";
    }
}

String PowerMeter::getMCBStatus() const {
    return stMCB1;
}

bool PowerMeter::read(Data &outData) {
    updateMCBStatus();

    const uint8_t result = node.readInputRegisters(0x0000, 10);

    if (result == node.ku8MBSuccess) {
        ledManager.setMode(LED_ON);
        Serial.println("[MODBUS] PZEM-016 connected");

        const uint32_t rawCurrent =
            ((uint32_t)node.getResponseBuffer(0x02) << 16) |
            node.getResponseBuffer(0x01);

        const uint32_t rawPower =
            ((uint32_t)node.getResponseBuffer(0x04) << 16) |
            node.getResponseBuffer(0x03);

        const uint32_t rawEnergy =
            ((uint32_t)node.getResponseBuffer(0x06) << 16) |
            node.getResponseBuffer(0x05);

        outData.voltage = node.getResponseBuffer(0x00) / 10.0f;
        outData.current = rawCurrent / 1000.0f;
        outData.power = rawPower / 10.0f;
        outData.energy = rawEnergy / 1000.0f;
        outData.frequency = node.getResponseBuffer(0x07) / 10.0f;
        outData.pf = node.getResponseBuffer(0x08) / 100.0f;
        outData.mcb1 = stMCB1;
        outData.status = "OK";

        return true;
    }

    Serial.print("[MODBUS] PZEM-016 failed, error: 0x");
    Serial.println(result, HEX);

    ledManager.setMode(LED_BLINK_FAST);

    outData.voltage = 0;
    outData.current = 0;
    outData.power = 0;
    outData.frequency = 0;
    outData.pf = 0;
    outData.energy = 0;
    outData.mcb1 = stMCB1;

    if (result == 0xE2) {
        outData.status = "PZEM_NOT_FOUND";
    } else if (result == 0xE3) {
        outData.status = "CRC_ERROR";
    } else {
        outData.status = "MODBUS_ERR";
    }

    return false;
}

void PowerMeter::resetEnergy() {
    const uint8_t resetCommand = 0x42;
    uint16_t crc = 0xFFFF;

    crc = crc16_update(crc, PZEM_SLAVE_ADDR);
    crc = crc16_update(crc, resetCommand);

    Serial.println("[PZEM] Resetting energy...");

    digitalWrite(MAX485_RE, HIGH);
    digitalWrite(MAX485_DE, HIGH);
    delay(10);

    pzemSerial.write(PZEM_SLAVE_ADDR);
    pzemSerial.write(resetCommand);
    pzemSerial.write(lowByte(crc));
    pzemSerial.write(highByte(crc));

    delay(10);

    Serial.print("[PZEM] Response: ");
    while (pzemSerial.available() > 0) {
        Serial.print(pzemSerial.read(), HEX);
        Serial.print(",");
    }
    Serial.println();

    delay(1000);

    digitalWrite(MAX485_RE, LOW);
    digitalWrite(MAX485_DE, LOW);
}
