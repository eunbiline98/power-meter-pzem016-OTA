#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

class OtaManager {
public:
    void checkBootState();
    void begin();
    void handleClient();
    void confirmBootIfReady();

private:
    unsigned long bootValidationStartedAt = 0;
    bool bootValidationPending = false;
};

#endif
