#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>

#include "Config/Config.h"

#ifdef USE_WIFI
#include <WiFi.h>
#elif defined(USE_ETHERNET)
#include <EthernetSPI2.h>
#else
#error "Pilih salah satu mode jaringan: USE_WIFI atau USE_ETHERNET"
#endif

class NetworkManager {
public:
    void begin();
    void loop();
    bool isConnected();
    String getRSSIString();
    IPAddress localIP();

#ifdef USE_WIFI
    WiFiClient &getClient();
#elif defined(USE_ETHERNET)
    EthernetClient &getClient();
#endif

private:
#ifdef USE_WIFI
    WiFiClient netClient;
    void beginWiFi();
#elif defined(USE_ETHERNET)
    EthernetClient netClient;
    void beginEthernet();
#endif
};

#endif