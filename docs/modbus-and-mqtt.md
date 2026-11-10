# Pembacaan Modbus, LED, dan MQTT

[Kembali ke README](../README.md)

## 9. Cara mengambil data Modbus

### Konfigurasi komunikasi

| Parameter       | Nilai                        |
| --------------- | ---------------------------- |
| Protokol        | Modbus RTU                   |
| Transport       | RS485                        |
| ESP32           | Master                       |
| PZEM            | Slave                        |
| Serial          | `Serial2`                    |
| Baud rate       | 9600                         |
| Data bit        | 8                            |
| Parity          | None                         |
| Stop bit        | 2                            |
| Slave ID        | `0x01`                       |
| Function code   | `0x04`, Read Input Registers |
| Alamat awal     | `0x0000`                     |
| Jumlah register | 10                           |

Kode pembacaan:

```cpp
pzemSerial.begin(9600, SERIAL_8N2, MAX485_RO, MAX485_DI);
node.begin(PZEM_SLAVE_ADDR, pzemSerial);

uint8_t result = node.readInputRegisters(0x0000, 10);
```

`10` berarti sepuluh register 16 bit, sehingga data respons berukuran 20 byte.

### Perbedaan alamat

- `0x01`: alamat unit/slave PZEM di bus RS485.
- `0x0000`: alamat register data di dalam PZEM.
- IP ESP32: alamat jaringan untuk MQTT/OTA.

Ketiganya memiliki fungsi berbeda.

### Register dan konversi

| Parameter         | Register                    | Konversi                      | Satuan       |
| ----------------- | --------------------------- | ----------------------------- | ------------ |
| Tegangan          | `0x0000`                    | raw / 10                      | V            |
| Arus              | `0x0001` low, `0x0002` high | uint32 / 1000                 | A            |
| Daya              | `0x0003` low, `0x0004` high | uint32 / 10                   | W            |
| Energi            | `0x0005` low, `0x0006` high | uint32 / 1000                 | kWh          |
| Frekuensi         | `0x0007`                    | raw / 10                      | Hz           |
| Faktor daya       | `0x0008`                    | raw / 100                     | Tanpa satuan |
| Register tambahan | `0x0009`                    | Dibaca tetapi tidak digunakan | —            |

Untuk parameter 32 bit, register beralamat lebih rendah adalah low word:

```cpp
uint32_t rawCurrent =
    (uint32_t(node.getResponseBuffer(0x02)) << 16) |
    node.getResponseBuffer(0x01);

float currentA = rawCurrent / 1000.0f;
```

Contoh konversi:

| Raw              | Hasil        |
| ---------------- | ------------ |
| Tegangan `2301`  | `230.1 V`    |
| Arus `1234`      | `1.234 A`    |
| Daya `1205`      | `120.5 W`    |
| Energi `12345`   | `12.345 kWh` |
| Frekuensi `500`  | `50 Hz`      |
| Faktor daya `94` | `0.94`       |

### Frame request

```text
01 04 00 00 00 0A CRC_LO CRC_HI
```

| Bagian  | Arti                                |
| ------- | ----------------------------------- |
| `01`    | Slave ID                            |
| `04`    | Read Input Registers                |
| `00 00` | Alamat awal                         |
| `00 0A` | Jumlah register: 10                 |
| CRC     | CRC16 Modbus, low byte lebih dahulu |

Respons normal:

```text
01 04 14 [20 byte data] CRC_LO CRC_HI
```

`0x14` berarti panjang data 20 byte.

Pada setiap register 16 bit, high byte dikirim terlebih dahulu. Untuk nilai 32 bit, urutan register tetap mengikuti tabel low word/high word di atas.

Library ModbusMaster menangani framing dan pemeriksaan CRC pembacaan.

### Membaca memakai Modbus Poll atau QModMaster

1. Hubungkan USB-RS485 ke PZEM.
2. Pilih Modbus RTU serial.
3. Atur 9600 baud, 8N2.
4. Pilih slave ID `1`.
5. Pilih function `04 Read Input Registers`.
6. Atur start address/offset `0`, quantity `10`.
7. Terapkan faktor skala dan urutan word pada tabel.

Beberapa tool menampilkan offset `0` sebagai referensi input register `30001`. Periksa mode penomoran agar alamat tidak bergeser satu register.

Jangan menjalankan ESP32 dan USB-RS485 sebagai dua master aktif pada bus yang sama.

### Jika pembacaan gagal

Nilai tegangan, arus, daya, energi, frekuensi, dan faktor daya diisi nol.

Status internal:

| Error   | Status internal  |
| ------- | ---------------- |
| `0xE2`  | `PZEM_NOT_FOUND` |
| `0xE3`  | `CRC_ERROR`      |
| Lainnya | `MODBUS_ERR`     |

Status internal ini belum dimasukkan ke payload MQTT aktif.

## 10. LED indikator

| Kondisi                   | Pola GPIO 22          |
| ------------------------- | --------------------- |
| Inisialisasi PZEM         | Toggle setiap 1000 ms |
| Pembacaan Modbus berhasil | Menyala terus         |
| Pembacaan Modbus gagal    | Toggle setiap 250 ms  |

LED menunjukkan hasil komunikasi PZEM, bukan keberhasilan MQTT atau koneksi Ethernet.

## 11. MQTT

Koneksi menggunakan port 1883 dan client TCP dari jaringan terpilih. Implementasi saat ini tidak menggunakan TLS.

Setelah koneksi berhasil:

1. Counter kegagalan direset.
2. Publish status `Online`.
3. Publish informasi jaringan.
4. Subscribe topic reset energi.
5. Subscribe topic perintah umum.

Retry dilakukan paling cepat setiap 5 detik. Setelah 200 kegagalan berturut-turut, ESP32 restart.

### Topic

Arah dilihat dari ESP32.

| Topic                   | Arah                      | Isi                             |
| ----------------------- | ------------------------- | ------------------------------- |
| `pow/test/status/lwt`   | Publish/Last Will         | `Online` atau `Offline`         |
| `pow/test/network/stat` | Publish                   | JSON IP dan RSSI                |
| `pow/test/data/stat`    | Publish                   | JSON parameter listrik          |
| `pow/test/rst/cmnd`     | Subscribe dan publish ack | `1` reset, `0` selesai          |
| `pow/test/rst/stat`     | Publish                   | `0` setelah prosedur reset      |
| `pow/test/data/cmnd`    | Subscribe                 | Belum memiliki handler tindakan |

Status jaringan, LWT, dan ack reset dipublish retained QoS 1. Telemetri listrik memakai parameter default library.

### Payload listrik

```json
{
  "POWER": 120.5,
  "VOLT": 230.1,
  "AMP": 0.56,
  "FREQ": 50.0,
  "PF": 0.94,
  "ENERGY": 12.345,
  "MCB1": "ON",
  "STATUS": "ACTIVE"
}
```

`STATUS` dihitung dari tegangan:

- `VOLT > 0`: `ACTIVE`.
- `VOLT <= 0`: `DEACTIVE`.

Karena kegagalan Modbus juga menghasilkan tegangan nol, `DEACTIVE` tidak membedakan listrik padam dari komunikasi PZEM gagal.

### Payload jaringan

```json
{
  "ip": "192.168.8.159",
  "rssi": "0"
}
```

`rssi` berupa string:

- Ethernet: selalu `"0"`.
- Wi-Fi: nilai RSSI dalam dBm yang dikonversi menjadi string.

### Reset energi

Publish payload `1` ke:

```text
pow/test/rst/cmnd
```

Firmware:

1. Mengirim perintah khusus PZEM `0x42`.
2. Menyertakan slave ID dan CRC16.
3. Menjalankan reset dua kali.
4. Memberi jeda 2 detik setelah masing-masing reset.
5. Publish `0` ke topic reset state dan command.

Frame reset:

```text
[slave ID] [42] [CRC low] [CRC high]
```

Prosedur menahan loop sekitar 6 detik ditambah waktu I/O.

Respons reset dicetak ke serial tetapi belum divalidasi sebagai keberhasilan reset. Ack `0` menunjukkan prosedur telah dijalankan; verifikasi hasil dari pembacaan energi berikutnya.

Hindari menyimpan perintah reset `1` sebagai retained message karena dapat dijalankan lagi saat perangkat subscribe kembali.
