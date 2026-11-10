#include "NetworkManager.h"

namespace {

unsigned long lastReconnectAttempt = 0;
unsigned long lastNetPrint = 0;

constexpr unsigned long WIFI_RECONNECT_INTERVAL = 10000;
constexpr unsigned long NET_PRINT_INTERVAL = 5000;

#ifdef USE_ETHERNET
byte ethMac[6];

void generateEthernetMAC() {
    const uint64_t chipId = ESP.getEfuseMac();

    ethMac[0] = 0x02; // Locally administered MAC.
    ethMac[1] = 0xAA;
    ethMac[2] = 0xCD;
    ethMac[3] = (chipId >> 16) & 0xFF;
    ethMac[4] = (chipId >> 8) & 0xFF;
    ethMac[5] = chipId & 0xFF;

    Serial.print("[NET] Ethernet MAC: ");

    for (int i = 0; i < 6; i++) {
        if (ethMac[i] < 16) {
            Serial.print("0");
        }

        Serial.print(ethMac[i], HEX);

        if (i < 5) {
            Serial.print(":");
        }
    }

    Serial.println();
}
#endif

} // namespace

void NetworkManager::begin() {
#ifdef USE_WIFI
    beginWiFi();
#elif defined(USE_ETHERNET)
    beginEthernet();
#endif
}

void NetworkManager::loop() {
    const unsigned long now = millis();

#ifdef USE_WIFI
    if (WiFi.status() != WL_CONNECTED) {
        if (now - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;

            Serial.println("[NET] WiFi disconnected, reconnecting...");
            WiFi.disconnect();
            WiFi.begin(SSID, PASS);
        }
    } else if (now - lastNetPrint >= NET_PRINT_INTERVAL) {
        lastNetPrint = now;
        Serial.print("[NET] WiFi OK | IP: ");
        Serial.print(WiFi.localIP());
        Serial.print(" | RSSI: ");
        Serial.println(WiFi.RSSI());
    }

#elif defined(USE_ETHERNET)
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        if (now - lastNetPrint >= NET_PRINT_INTERVAL) {
            lastNetPrint = now;
            Serial.println("[NET] Ethernet hardware not found");
        }

        return;
    }

    if (Ethernet.linkStatus() == LinkOFF) {
        if (now - lastNetPrint >= NET_PRINT_INTERVAL) {
            lastNetPrint = now;
            Serial.println("[NET] Ethernet cable not connected");
        }
    } else if (now - lastNetPrint >= NET_PRINT_INTERVAL) {
        lastNetPrint = now;
        Serial.print("[NET] Ethernet OK | IP: ");
        Serial.println(Ethernet.localIP());
    }
#endif
}

bool NetworkManager::isConnected() {
#ifdef USE_WIFI
    return WiFi.status() == WL_CONNECTED;
#elif defined(USE_ETHERNET)
    return Ethernet.linkStatus() != LinkOFF;
#else
    return false;
#endif
}

String NetworkManager::getRSSIString() {
#ifdef USE_WIFI
    return String(WiFi.RSSI());
#else
    return "0";
#endif
}

IPAddress NetworkManager::localIP() {
#ifdef USE_WIFI
    return WiFi.localIP();
#elif defined(USE_ETHERNET)
    return Ethernet.localIP();
#else
    return IPAddress(0, 0, 0, 0);
#endif
}

#ifdef USE_WIFI
WiFiClient &NetworkManager::getClient() {
    return netClient;
}
#endif

#ifdef USE_ETHERNET
EthernetClient &NetworkManager::getClient() {
    return netClient;
}
#endif

#ifdef USE_WIFI
void NetworkManager::beginWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    WiFi.begin(SSID, PASS);

    lastReconnectAttempt = millis();

    Serial.println("[NET] WiFi connecting...");
}
#endif

#ifdef USE_ETHERNET
void NetworkManager::beginEthernet() {
    generateEthernetMAC();

    Ethernet.init(ETH_CS_PIN);

    Serial.println("[NET] Ethernet DHCP starting...");
    Ethernet.begin(ethMac);

    const unsigned long startTime = millis();

    while (Ethernet.localIP() == IPAddress(0, 0, 0, 0)) {
        if (millis() - startTime >= 10000) {
            Serial.println("[NET] Ethernet DHCP failed, using static IP");
            Ethernet.begin(ethMac, ETH_IP, ETH_DNS, ETH_GATEWAY, ETH_SUBNET);
            break;
        }

        delay(100);
    }

    Serial.print("[NET] Ethernet IP: ");
    Serial.println(Ethernet.localIP());

    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("[NET] Ethernet hardware not found");
        return;
    }

    if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("[NET] Ethernet cable not connected");
        return;
    }

    Serial.println("[NET] Ethernet connected");
    Serial.print("[NET] Local IP: ");
    Serial.println(Ethernet.localIP());
}
#endif
