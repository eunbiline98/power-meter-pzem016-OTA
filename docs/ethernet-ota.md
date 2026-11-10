# Ethernet OTA update

The firmware upload page uses the local W5500 Ethernet connection and is served
at `http://<device-ip>/`. Upload a firmware `.bin` file built for this project.
The server accepts raw binary uploads only; it does not accept filesystem images.

Run a PlatformIO build to generate `firmware.bin` in the project root. The
original build output remains in `.pio/build/esp32doit-devkit-v1/firmware.bin`.

## Enable authenticated uploads

Add separate credentials to the ignored `include/Config/secrets.h` file:

```cpp
#define OTA_USERNAME "a-unique-username"
#define OTA_PASSWORD "a-long-random-password"
```

The upload server stays disabled if either credential is missing or empty. Do
not commit `secrets.h`, reuse MQTT credentials, or expose TCP port 80 outside a
trusted local network. HTTP Basic authentication is not encrypted, so use this
only on a trusted LAN.

## OTA slots and rollback

`partitions.csv` reserves two application slots. An interrupted or invalid
upload leaves the running firmware selected. After a successful upload, the
new firmware is observed for 60 seconds before its rollback record is cleared.
If the device restarts during this validation window, startup selects the
previous application slot and restarts.

Because this project now uses a custom partition table, flash the device over
USB once after applying this change. A browser OTA update replaces firmware
only; it cannot change the device's partition table.

Rollback metadata is kept in NVS. This application-level recovery requires the
new firmware to reach `setup()` to detect a failed boot; it is not a substitute
for an ESP-IDF bootloader built with `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`.
Keep serial flashing available for recovery from failures that occur before
application startup.
