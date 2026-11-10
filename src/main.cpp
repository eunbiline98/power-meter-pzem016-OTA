#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <MQTT.h>

#include "Config/Config.h"
#include "LedManager.h"
#include "NetworkManager.h"
#include "PowerMeter.h"

NetworkManager network;
LedManager ledManager;
MQTTClient client(512);
PowerMeter meter;

unsigned long lastPublish = 0;
int mqttFailCounter = 0;

// void publishPowerData()
// {
//   PowerMeter::Data data;
//   meter.read(data);

//   JsonDocument doc;
//   doc["POWER"] = data.power;
//   doc["VOLT"] = data.voltage;
//   doc["AMP"] = data.current;
//   doc["FREQ"] = data.frequency;
//   doc["PF"] = data.pf;
//   doc["ENERGY"] = data.energy;
//   doc["MCB1"] = data.mcb1;
//   doc["ST"] = data.status;

//   char buffer[256];
//   serializeJson(doc, buffer, sizeof(buffer));

//   client.publish(MQTT_ELEC_STATE, buffer);

//   Serial.println("[MQTT] Publish:");
//   Serial.println(buffer);
// }

void publishPowerData()
{
  PowerMeter::Data data;
  meter.read(data);

  JsonDocument doc;

  doc["POWER"] = data.power;
  doc["VOLT"] = data.voltage;
  doc["AMP"] = data.current;
  doc["FREQ"] = data.frequency;
  doc["PF"] = data.pf;
  doc["ENERGY"] = data.energy;
  doc["MCB1"] = data.mcb1;

  // Status berdasarkan tegangan
  if (data.voltage <= 0)
  {
    doc["STATUS"] = "DEACTIVE";
  }
  else
  {
    doc["STATUS"] = "ACTIVE";
  }

  char buffer[256];
  serializeJson(doc, buffer, sizeof(buffer));

  client.publish(MQTT_ELEC_STATE, buffer);

  Serial.println("[MQTT] Publish:");
  Serial.println(buffer);
}

void publishNetworkData()
{
  JsonDocument doc;

  doc["ip"] = network.localIP().toString();
  doc["rssi"] = network.getRSSIString();

  char buffer[192];
  serializeJson(doc, buffer, sizeof(buffer));

  client.publish(MQTT_NETWORK_STAT, buffer, true, 1);

  Serial.println("[MQTT] Network:");
  Serial.println(buffer);
}

void mqttCallback(String &topic, String &payload)
{
  Serial.print("[MQTT] Topic: ");
  Serial.println(topic);

  Serial.print("[MQTT] Payload: ");
  Serial.println(payload);

  payload.replace("\"", "");

  if (topic == MQTT_ELEC_RST_CMD)
  {
    if (payload == "1")
    {
      for (int i = 0; i < 2; i++)
      {
        meter.resetEnergy();
        delay(2000);
      }

      client.publish(MQTT_ELEC_RST_STATE, "0", true, 1);
      client.publish(MQTT_ELEC_RST_CMD, "0", true, 1);
    }
    else if (payload == "0")
    {
      // Do nothing, just acknowledge the reset command
    }
  }
}

void reconnectMQTT()
{
  while (!client.connected())
  {
    Serial.print("[MQTT] Attempting connection...");

    bool ok = client.connect(
        MQTT_CLIENT_ID,
        MQTT_USERNAME,
        MQTT_PASSWORD);

    if (ok)
    {
      Serial.println("connected");
      mqttFailCounter = 0;

      client.publish(MQTT_STATUS, LWT_ONLINE, true, 1);
      publishNetworkData();

      client.subscribe(MQTT_ELEC_RST_CMD);
      client.subscribe(MQTT_ELEC_CMD);

      Serial.println("[MQTT] Subscribed topics");
    }
    else
    {
      Serial.print("failed, error=");
      Serial.print(client.lastError());
      Serial.print(", returnCode=");
      Serial.println(client.returnCode());

      mqttFailCounter++;

      if (mqttFailCounter >= MQTT_FAIL_RESTART_LIMIT)
      {
        Serial.println("[MQTT] Too many failures, restarting ESP...");
        ESP.restart();
      }

      delay(MQTT_RETRY_INTERVAL_MS);
    }
  }
}

void setupOTA()
{
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("[OTA] Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] End");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);

    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });

  ArduinoOTA.begin();
  Serial.println("[OTA] Ready");
}

void setup()
{
  Serial.begin(115200);

  pinMode(MCB_1, INPUT);
  pinMode(LED_INDI, OUTPUT);

  digitalWrite(LED_INDI, LOW);

  ledManager.begin();
  ledManager.setMode(LED_BLINK_SLOW);

  meter.begin();
  network.begin();

  // setupOTA();

  client.begin(MQTT_SERVER, MQTT_PORT, network.getClient());
  client.setWill(MQTT_STATUS, LWT_OFFLINE, true, 1); // Mengatur Last Will and Testament (LWT)
  client.onMessage(mqttCallback);

  client.setWill(MQTT_STATUS, LWT_OFFLINE, true, 1);

  Serial.println("[SYS] Setup complete");
}

void loop()
{
  // ArduinoOTA.handle();
  network.loop();
  ledManager.loop();
  network.loop();

  if (!client.connected())
  {
    reconnectMQTT();
  }

  client.loop();

  unsigned long now = millis();

  if (now - lastPublish >= PUBLISH_INTERVAL_MS)
  {
    publishPowerData();
    publishNetworkData();
    lastPublish = now;
  }
}