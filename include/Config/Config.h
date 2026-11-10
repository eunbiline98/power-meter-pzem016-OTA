#ifndef CONFIG_H
#define CONFIG_H

#include "secrets.h"
#include <Arduino.h>
#include <SPI.h>

// ===================== PILIH JARINGAN =====================
// Aktifkan salah satu saja
// #define USE_WIFI
#define USE_ETHERNET

// ===================== PINOUT =====================
// RS485 - PZEM016
#define MAX485_DE 26
#define MAX485_RE 25
#define MAX485_RO 33
#define MAX485_DI 27

#define MCB_1 4
#define LED_DATA 2
#define LED_INDI 22

// Jika pakai Ethernet module SPI (W5500 / W5100)
#define ETH_CS_PIN 15

// ===================== WIFI CONFIG =====================
static const char *SSID = WIFI_SSID;
static const char *PASS = WIFI_PASS;

// ===================== MQTT CONFIG =====================
static const char *MQTT_SERVER = MQTT_SERVER_ADDR;
static const uint16_t MQTT_PORT = 1883;

static const char *MQTT_STATUS = "pow/test/status/lwt";
static const char *MQTT_NETWORK_STAT = "pow/test/network/stat";

static const char *MQTT_ELEC_CMD = "pow/test/data/cmnd";
static const char *MQTT_ELEC_STATE = "pow/test/data/stat";

static const char *MQTT_ELEC_RST_CMD = "pow/test/rst/cmnd";
static const char *MQTT_ELEC_RST_STATE = "pow/test/rst/stat";

static const char *MQTT_CLIENT_ID = "POW_TEST_1P";
static const char *MQTT_USERNAME = MQTT_USER;
static const char *MQTT_PASSWORD = MQTT_PASS;

// ===================== ETHERNET CONFIG =====================
static IPAddress ETH_IP(192, 168, 8, 159);
static IPAddress ETH_DNS(8, 8, 8, 8);
static IPAddress ETH_GATEWAY(192, 168, 8, 1);
static IPAddress ETH_SUBNET(255, 255, 255, 0);

// ===================== PZEM CONFIG =====================
static const uint8_t PZEM_SLAVE_ADDR = 0x01;

// ===================== TIMING =====================
static const unsigned long PUBLISH_INTERVAL_MS = 5000;
static const unsigned long MQTT_RETRY_INTERVAL_MS = 5000;
static const int MQTT_FAIL_RESTART_LIMIT = 200;

// ===================== MQTT LWT =====================
static const char *LWT_ONLINE = "Online";
static const char *LWT_OFFLINE = "Offline";

#endif