# Perangkat, struktur proyek, dan konfigurasi

[Kembali ke README](../README.md)

## 3. Perangkat dan wiring

Perangkat utama:

- ESP32 DOIT DevKit V1 dengan konfigurasi flash 4 MB.
- PZEM-016.
- Transceiver RS485 dengan level logika sesuai ESP32.
- Modul Ethernet W5500.
- Input MCB yang telah dikondisikan menjadi sinyal digital.
- LED indikator eksternal dan resistor.

### Pin RS485 dan input

| Konstanta   | GPIO | Fungsi                                  |
| ----------- | ---- | --------------------------------------- |
| `MAX485_DE` | 26   | Enable transmisi RS485                  |
| `MAX485_RE` | 25   | Enable penerimaan `/RE`, aktif LOW      |
| `MAX485_RO` | 33   | RX ESP32, terhubung ke RO transceiver   |
| `MAX485_DI` | 27   | TX ESP32, terhubung ke DI transceiver   |
| `MCB_1`     | 4    | Input status MCB                        |
| `LED_INDI`  | 22   | LED status komunikasi PZEM              |
| `LED_DATA`  | 2    | Didefinisikan, belum digunakan aplikasi |

Hubungkan terminal A/B transceiver ke A/B PZEM sesuai penandaan perangkat.

`MCB_1` menggunakan `INPUT`, bukan `INPUT_PULLUP`. Rangkaian eksternal harus memastikan level input tidak floating.

Logika status MCB:

| Level GPIO 4 | Status |
| ------------ | ------ |
| LOW          | `ON`   |
| HIGH         | `OFF`  |

Input ini hanya membaca status. Firmware tidak menyediakan output untuk menyalakan atau mematikan MCB.

### Pin W5500

Library lokal `EthernetSPI2` menggunakan:

```cpp
SPIClass SPI2(HSPI);
SPI2.begin();
```

Pada ESP32 klasik, pin default HSPI adalah:

| Sinyal W5500 | GPIO |
| ------------ | ---- |
| SCK          | 14   |
| MISO         | 12   |
| MOSI         | 13   |
| CS/SCS       | 15   |
| GND          | GND  |

CS diatur melalui `ETH_CS_PIN`.

Pastikan pin default HSPI sesuai board/core yang digunakan. GPIO 12 dan 15 merupakan pin strapping ESP32; perhatikan level eksternal saat boot jika perangkat sulit menyala.

Tegangan catu modul mengikuti spesifikasi breakout. GPIO ESP32 tidak boleh menerima sinyal 5 V atau tegangan listrik AC secara langsung. Gunakan isolasi yang sesuai untuk status MCB dan instalasi PZEM.

## 4. Struktur proyek

```text
.
├── platformio.ini
├── partitions.csv
├── include/
│   ├── Config/
│   │   ├── Config.h
│   │   └── secrets.example.h
│   ├── NetworkManager.h
│   ├── PowerMeter.h
│   ├── LedManager.h
│   └── OtaManager.h
├── src/
│   ├── main.cpp
│   ├── NetworkManager.cpp
│   ├── PowerMeter.cpp
│   ├── LedManager.cpp
│   └── OtaManager.cpp
├── lib/
│   └── EthernetSPI2/
├── scripts/
│   └── copy_firmware.py
├── docs/
│   └── ethernet-ota.md
└── test/
```

| File                 | Tanggung jawab                                          |
| -------------------- | ------------------------------------------------------- |
| `main.cpp`           | Setup, loop, MQTT, payload JSON                         |
| `NetworkManager.cpp` | Inisialisasi dan pemantauan jaringan                    |
| `PowerMeter.cpp`     | Pembacaan Modbus, konversi data, reset energi           |
| `LedManager.cpp`     | Pola indikator LED                                      |
| `OtaManager.cpp`     | HTTP upload, penulisan firmware, metadata rollback      |
| `Config.h`           | Pin, pilihan jaringan, topic, interval, konfigurasi OTA |
| `partitions.csv`     | Layout flash                                            |
| `copy_firmware.py`   | Menyalin binary aplikasi ke root proyek                 |

## 5. Konfigurasi awal

### Kredensial

Salin:

```sh
cp include/Config/secrets.example.h include/Config/secrets.h
```

Isi konfigurasi:

```cpp
#define WIFI_SSID "nama-wifi"
#define WIFI_PASS "password-wifi"

#define MQTT_USER "mqtt-user"
#define MQTT_PASS "mqtt-password"
#define MQTT_SERVER_ADDR "192.168.8.10"
```

Pada Windows, file dapat disalin melalui Explorer.

Definisi Wi-Fi tetap perlu tersedia karena direferensikan oleh `Config.h`, walaupun mode Ethernet digunakan.

`secrets.h` sudah diabaikan oleh Git.

### Kredensial OTA: perilaku kode saat ini

`Config.h` mendefinisikan langsung:

```cpp
#define OTA_USERNAME "admin"
#define OTA_PASSWORD "admin"
```

Ganti nilai tersebut sebelum penggunaan rutin.

Template secrets dan dokumentasi OTA menyarankan penempatan kredensial di `secrets.h`, tetapi `Config.h` saat ini mendefinisikan ulang keduanya setelah include secrets.

Jika ingin menyimpan kredensial OTA hanya di `secrets.h`, pindahkan kedua definisi dari `Config.h` ke `secrets.h`. Hindari definisi ganda.

### Pilihan jaringan

Aktifkan tepat satu mode di `Config.h`.

Ethernet, default:

```cpp
// #define USE_WIFI
#define USE_ETHERNET
```

Wi-Fi:

```cpp
#define USE_WIFI
// #define USE_ETHERNET
```

Tidak ada perpindahan otomatis Ethernet ke Wi-Fi. Mengaktifkan keduanya bukan konfigurasi yang didukung.

### Parameter default

| Parameter            | Nilai           |
| -------------------- | --------------- |
| Port MQTT            | 1883            |
| MQTT Client ID       | `POW_TEST_1P`   |
| IP fallback          | `192.168.8.159` |
| Gateway fallback     | `192.168.8.1`   |
| Subnet               | `255.255.255.0` |
| DNS fallback         | `8.8.8.8`       |
| PZEM slave ID        | `0x01`          |
| Interval publish     | 5000 ms         |
| Interval retry MQTT  | 5000 ms         |
| Batas kegagalan MQTT | 200 percobaan   |

Gunakan Client ID unik untuk setiap perangkat yang mengakses broker yang sama.
