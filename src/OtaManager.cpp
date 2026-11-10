#include "OtaManager.h"

#include <Arduino.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Config/Config.h"

#ifdef USE_ETHERNET
#include <EthernetSPI2.h>
#endif

namespace {

constexpr char ROLLBACK_NAMESPACE[] = "ota-rollback";
constexpr unsigned long BOOT_VALIDATION_PERIOD_MS = 60000;
constexpr unsigned long REQUEST_TIMEOUT_MS = 5000;
constexpr size_t MAX_HEADER_BYTES = 1024;
constexpr size_t MAX_HEADER_LINE_BYTES = 256;

#if defined(USE_ETHERNET)
class OtaEthernetServer : public EthernetServer {
public:
    explicit OtaEthernetServer(uint16_t port) : EthernetServer(port) {}

    void begin(uint16_t port = 0) override {
        (void)port;
        EthernetServer::begin();
    }
};

OtaEthernetServer otaServer(80);

const char UPDATE_PAGE[] =
    "<!doctype html><html lang=\"en\"><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ESP32 firmware update</title>"
    "<style>body{font:16px sans-serif;max-width:38rem;margin:3rem auto;padding:0 1rem}"
    "label{display:block;margin:1rem 0}input,button{padding:.6rem}#status{white-space:pre-wrap}"
    "</style><h1>Firmware update</h1>"
    "<p>Choose the firmware .bin file built for this device.</p>"
    "<form id=\"upload\"><label>Username <input name=\"username\" autocomplete=\"username\" required></label>"
    "<label>Password <input name=\"password\" type=\"password\" autocomplete=\"current-password\" required></label>"
    "<label>Firmware <input name=\"firmware\" type=\"file\" accept=\".bin,application/octet-stream\" required></label>"
    "<button>Upload firmware</button></form><p id=\"status\"></p>"
    "<script>document.querySelector('#upload').addEventListener('submit',async e=>{"
    "e.preventDefault();const f=e.target.firmware.files[0];if(!f)return;"
    "const s=document.querySelector('#status');s.textContent='Uploading...';"
    "const a=btoa(e.target.username.value+':'+e.target.password.value);"
    "try{const r=await fetch('/update',{method:'POST',headers:{Authorization:'Basic '+a,"
    "'Content-Type':'application/octet-stream'},body:f});"
    "s.textContent=await r.text();if(r.ok)s.textContent+='\\nDevice will restart.';}"
    "catch(_){s.textContent='Connection interrupted. Check the device before retrying.';}"
    "});</script></html>";

#endif

void clearRollbackRecord(Preferences &preferences) {
    if (preferences.putBool("pending", false) == 0) {
        Serial.println("[OTA] Could not clear rollback state");
    }
}

bool prepareRollbackRecord(uint8_t targetSubtype) {
    const esp_partition_t *runningPartition = esp_ota_get_running_partition();
    if (runningPartition == nullptr || runningPartition->subtype == targetSubtype) {
        Serial.println("[OTA] Cannot determine rollback partitions");
        return false;
    }

    Preferences preferences;
    if (!preferences.begin(ROLLBACK_NAMESPACE, false)) {
        Serial.println("[OTA] Cannot open rollback storage");
        return false;
    }

    const bool saved =
        preferences.putUChar("target", targetSubtype) == sizeof(uint8_t) &&
        preferences.putUChar("previous", runningPartition->subtype) == sizeof(uint8_t) &&
        preferences.putBool("started", false) != 0 &&
        preferences.putBool("pending", true) != 0;

    if (!saved) {
        clearRollbackRecord(preferences);
        Serial.println("[OTA] Cannot save rollback state");
    }

    preferences.end();
    return saved;
}

#if defined(USE_ETHERNET)
bool readLine(EthernetClient &client, char *line, size_t capacity, size_t &length) {
    length = 0;
    const unsigned long startedAt = millis();

    while (millis() - startedAt < REQUEST_TIMEOUT_MS) {
        while (client.available() > 0) {
            const int value = client.read();
            if (value < 0) {
                continue;
            }

            if (value == '\n') {
                if (length > 0 && line[length - 1] == '\r') {
                    --length;
                }
                line[length] = '\0';
                return true;
            }

            if (length + 1 >= capacity) {
                return false;
            }
            line[length++] = static_cast<char>(value);
        }

        if (!client.connected()) {
            return false;
        }
        delay(1);
    }

    return false;
}

void sendResponse(
    EthernetClient &client,
    const char *status,
    const char *contentType,
    const char *body) {
    client.print("HTTP/1.1 ");
    client.println(status);
    client.print("Content-Type: ");
    client.println(contentType);
    client.print("Content-Length: ");
    client.println(strlen(body));
    client.println("Connection: close");
    client.println("Cache-Control: no-store");
    client.println();
    client.print(body);
}

bool encodeBasicCredentials(char *encoded, size_t capacity) {
#if defined(OTA_USERNAME) && defined(OTA_PASSWORD)
    if (sizeof(OTA_USERNAME) <= 1 || sizeof(OTA_PASSWORD) <= 1) {
        return false;
    }

    char credentials[128];
    const int credentialsLength =
        snprintf(credentials, sizeof(credentials), "%s:%s", OTA_USERNAME, OTA_PASSWORD);
    if (credentialsLength <= 0 ||
        static_cast<size_t>(credentialsLength) >= sizeof(credentials)) {
        return false;
    }

    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const size_t required = ((static_cast<size_t>(credentialsLength) + 2) / 3) * 4 + 1;
    if (required > capacity) {
        return false;
    }

    size_t inputIndex = 0;
    size_t outputIndex = 0;
    while (inputIndex < static_cast<size_t>(credentialsLength)) {
        const uint32_t first = static_cast<uint8_t>(credentials[inputIndex++]);
        const bool hasSecond = inputIndex < static_cast<size_t>(credentialsLength);
        const uint32_t second = hasSecond
                                   ? static_cast<uint8_t>(credentials[inputIndex++])
                                   : 0;
        const bool hasThird = inputIndex < static_cast<size_t>(credentialsLength);
        const uint32_t third = hasThird
                                  ? static_cast<uint8_t>(credentials[inputIndex++])
                                  : 0;
        const uint32_t block = (first << 16) | (second << 8) | third;

        encoded[outputIndex++] = alphabet[(block >> 18) & 0x3F];
        encoded[outputIndex++] = alphabet[(block >> 12) & 0x3F];
        encoded[outputIndex++] = hasSecond ? alphabet[(block >> 6) & 0x3F] : '=';
        encoded[outputIndex++] = hasThird ? alphabet[block & 0x3F] : '=';
    }
    encoded[outputIndex] = '\0';
    return true;
#else
    (void)encoded;
    (void)capacity;
    return false;
#endif
}

bool constantTimeEquals(const char *left, const char *right) {
    const size_t leftLength = strlen(left);
    const size_t rightLength = strlen(right);
    const size_t compareLength = leftLength > rightLength ? leftLength : rightLength;
    uint8_t difference = static_cast<uint8_t>(leftLength ^ rightLength);

    for (size_t i = 0; i < compareLength; ++i) {
        const uint8_t leftByte = i < leftLength ? static_cast<uint8_t>(left[i]) : 0;
        const uint8_t rightByte = i < rightLength ? static_cast<uint8_t>(right[i]) : 0;
        difference |= leftByte ^ rightByte;
    }

    return difference == 0;
}

bool isAuthorized(const char *authorization) {
    if (authorization == nullptr) {
        return false;
    }

    char expected[192];
    char encoded[180];
    if (!encodeBasicCredentials(encoded, sizeof(encoded))) {
        return false;
    }

    const int length = snprintf(expected, sizeof(expected), "Basic %s", encoded);
    return length > 0 &&
           static_cast<size_t>(length) < sizeof(expected) &&
           constantTimeEquals(authorization, expected);
}

void handleRequest(EthernetClient &client) {
    char requestLine[MAX_HEADER_LINE_BYTES];
    size_t lineLength = 0;
    if (!readLine(client, requestLine, sizeof(requestLine), lineLength)) {
        sendResponse(client, "400 Bad Request", "text/plain", "Invalid request.\n");
        return;
    }

    char method[8] = {};
    char path[32] = {};
    char version[16] = {};
    if (sscanf(requestLine, "%7s %31s %15s", method, path, version) != 3) {
        sendResponse(client, "400 Bad Request", "text/plain", "Invalid request.\n");
        return;
    }

    char authorization[256] = {};
    char contentType[64] = {};
    uint32_t contentLength = 0;
    bool hasContentLength = false;
    bool hasAuthorization = false;
    bool hasContentType = false;
    size_t headerBytes = lineLength + 2;
    bool headersComplete = false;

    while (headerBytes <= MAX_HEADER_BYTES) {
        char line[MAX_HEADER_LINE_BYTES];
        if (!readLine(client, line, sizeof(line), lineLength)) {
            sendResponse(client, "400 Bad Request", "text/plain", "Invalid headers.\n");
            return;
        }
        headerBytes += lineLength + 2;

        if (lineLength == 0) {
            headersComplete = true;
            break;
        }

        char *separator = strchr(line, ':');
        if (separator == nullptr) {
            sendResponse(client, "400 Bad Request", "text/plain", "Invalid headers.\n");
            return;
        }

        *separator = '\0';
        char *value = separator + 1;
        while (*value == ' ' || *value == '\t') {
            ++value;
        }

        if (strcasecmp(line, "Authorization") == 0) {
            if (hasAuthorization || strlen(value) >= sizeof(authorization)) {
                sendResponse(client, "400 Bad Request", "text/plain", "Invalid headers.\n");
                return;
            }
            strcpy(authorization, value);
            hasAuthorization = true;
        } else if (strcasecmp(line, "Content-Type") == 0) {
            if (hasContentType || strlen(value) >= sizeof(contentType)) {
                sendResponse(client, "400 Bad Request", "text/plain", "Invalid headers.\n");
                return;
            }
            strcpy(contentType, value);
            hasContentType = true;
        } else if (strcasecmp(line, "Content-Length") == 0) {
            if (hasContentLength || value[0] == '\0') {
                sendResponse(client, "400 Bad Request", "text/plain", "Invalid content length.\n");
                return;
            }
            for (const char *digit = value; *digit != '\0'; ++digit) {
                if (!isdigit(static_cast<unsigned char>(*digit))) {
                    sendResponse(client, "400 Bad Request", "text/plain", "Invalid content length.\n");
                    return;
                }
            }

            char *end = nullptr;
            const unsigned long parsedLength = strtoul(value, &end, 10);
            if (end == value || *end != '\0' || parsedLength > UINT32_MAX) {
                sendResponse(client, "400 Bad Request", "text/plain", "Invalid content length.\n");
                return;
            }
            contentLength = static_cast<uint32_t>(parsedLength);
            hasContentLength = true;
        } else if (strcasecmp(line, "Transfer-Encoding") == 0) {
            sendResponse(client, "400 Bad Request", "text/plain", "Chunked requests are not supported.\n");
            return;
        }
    }

    if (!headersComplete) {
        sendResponse(client, "431 Request Header Fields Too Large", "text/plain", "Headers too large.\n");
        return;
    }

    if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
        sendResponse(client, "200 OK", "text/html; charset=utf-8", UPDATE_PAGE);
        return;
    }

    if (strcmp(method, "POST") != 0 || strcmp(path, "/update") != 0) {
        sendResponse(client, "404 Not Found", "text/plain", "Not found.\n");
        return;
    }

    if (!isAuthorized(authorization)) {
        sendResponse(client, "401 Unauthorized", "text/plain", "Authentication required.\n");
        return;
    }

    if (!hasContentLength || contentLength == 0 ||
        strcmp(contentType, "application/octet-stream") != 0) {
        sendResponse(client, "400 Bad Request", "text/plain", "Expected a binary firmware upload.\n");
        return;
    }

    const esp_partition_t *targetPartition = esp_ota_get_next_update_partition(nullptr);
    if (targetPartition == nullptr || contentLength > targetPartition->size) {
        sendResponse(client, "413 Content Too Large", "text/plain", "Firmware does not fit an OTA slot.\n");
        return;
    }

    if (!Update.begin(contentLength, U_FLASH)) {
        Serial.printf("[OTA] Update begin failed: %s\n", Update.errorString());
        sendResponse(client, "500 Internal Server Error", "text/plain", "Could not start firmware update.\n");
        return;
    }

    Serial.printf("[OTA] Receiving firmware (%lu bytes)\n",
                  static_cast<unsigned long>(contentLength));

    const size_t written = Update.writeStream(client);
    if (written != contentLength) {
        Update.abort();
        Serial.printf("[OTA] Incomplete upload: %lu of %lu bytes\n",
                      static_cast<unsigned long>(written),
                      static_cast<unsigned long>(contentLength));
        sendResponse(client, "400 Bad Request", "text/plain", "Upload was incomplete; current firmware was kept.\n");
        return;
    }

    if (!prepareRollbackRecord(targetPartition->subtype)) {
        Update.abort();
        sendResponse(client, "500 Internal Server Error", "text/plain", "Could not prepare rollback state.\n");
        return;
    }

    if (!Update.end()) {
        Serial.printf("[OTA] Firmware validation failed: %s\n", Update.errorString());
        Update.abort();

        Preferences preferences;
        if (preferences.begin(ROLLBACK_NAMESPACE, false)) {
            clearRollbackRecord(preferences);
            preferences.end();
        } else {
            Serial.println("[OTA] Could not clear rollback state after failed update");
        }

        sendResponse(client, "400 Bad Request", "text/plain", "Firmware validation failed; current firmware was kept.\n");
        return;
    }

    Serial.println("[OTA] Firmware accepted; restarting");
    sendResponse(client, "200 OK", "text/plain", "Firmware accepted.");
    delay(500);
    ESP.restart();
}
#endif

} // namespace

void OtaManager::checkBootState() {
    Preferences preferences;
    if (!preferences.begin(ROLLBACK_NAMESPACE, false)) {
        Serial.println("[OTA] Rollback storage unavailable");
        return;
    }

    if (!preferences.getBool("pending", false)) {
        preferences.end();
        return;
    }

    const uint8_t targetSubtype = preferences.getUChar("target", 0);
    const uint8_t previousSubtype = preferences.getUChar("previous", 0);
    const esp_partition_t *runningPartition = esp_ota_get_running_partition();

    if (runningPartition == nullptr) {
        Serial.println("[OTA] Cannot determine running firmware partition");
        preferences.end();
        return;
    }

    if (runningPartition->subtype != targetSubtype) {
        clearRollbackRecord(preferences);
        Serial.println("[OTA] Update was not selected; keeping current firmware");
        preferences.end();
        return;
    }

    if (!preferences.getBool("started", false)) {
        if (preferences.putBool("started", true) == 0) {
            Serial.println("[OTA] Cannot record firmware boot attempt");
        } else {
            bootValidationStartedAt = millis();
            bootValidationPending = true;
            Serial.println("[OTA] New firmware boot validation started");
        }
        preferences.end();
        return;
    }

    const esp_partition_t *previousPartition =
        esp_partition_find_first(
            ESP_PARTITION_TYPE_APP,
            static_cast<esp_partition_subtype_t>(previousSubtype),
            nullptr);
    if (previousPartition != nullptr &&
        esp_ota_set_boot_partition(previousPartition) == ESP_OK) {
        Serial.println("[OTA] New firmware did not pass boot validation; rolling back");
        preferences.end();
        ESP.restart();
        delay(1000);
        return;
    }

    Serial.println("[OTA] Rollback failed; continuing with current firmware");
    clearRollbackRecord(preferences);
    preferences.end();
}

void OtaManager::begin() {
#if defined(USE_ETHERNET)
#if defined(OTA_USERNAME) && defined(OTA_PASSWORD)
    if (sizeof(OTA_USERNAME) <= 1 || sizeof(OTA_PASSWORD) <= 1) {
        Serial.println("[OTA] Disabled: set separate OTA credentials in secrets.h");
        return;
    }

    char encodedCredentials[180];
    if (!encodeBasicCredentials(encodedCredentials, sizeof(encodedCredentials))) {
        Serial.println("[OTA] Disabled: OTA credentials are too long");
        return;
    }

    otaServer.begin();
    Serial.print("[OTA] Ethernet upload page: http://");
    Serial.print(Ethernet.localIP());
    Serial.println("/");
#else
    Serial.println("[OTA] Disabled: set separate OTA credentials in secrets.h");
#endif
#else
    Serial.println("[OTA] Disabled: Ethernet is required for this upload server");
#endif
}

void OtaManager::handleClient() {
#if defined(USE_ETHERNET) && defined(OTA_USERNAME) && defined(OTA_PASSWORD)
    if (sizeof(OTA_USERNAME) <= 1 || sizeof(OTA_PASSWORD) <= 1) {
        return;
    }

    EthernetClient client = otaServer.available();
    if (!client) {
        return;
    }

    handleRequest(client);
    delay(1);
    client.stop();
#endif
}

void OtaManager::confirmBootIfReady() {
    if (!bootValidationPending ||
        millis() - bootValidationStartedAt < BOOT_VALIDATION_PERIOD_MS) {
        return;
    }

    Preferences preferences;
    if (!preferences.begin(ROLLBACK_NAMESPACE, false)) {
        Serial.println("[OTA] Cannot open rollback storage to confirm firmware");
        return;
    }

    const esp_partition_t *runningPartition = esp_ota_get_running_partition();
    const uint8_t targetSubtype = preferences.getUChar("target", 0);
    if (!preferences.getBool("pending", false) ||
        runningPartition == nullptr ||
        runningPartition->subtype != targetSubtype) {
        preferences.end();
        bootValidationPending = false;
        return;
    }

    if (preferences.putBool("pending", false) != 0) {
        Serial.println("[OTA] Firmware passed boot validation");
        bootValidationPending = false;
    } else {
        Serial.println("[OTA] Could not confirm firmware; rollback remains armed");
    }
    preferences.end();
}
