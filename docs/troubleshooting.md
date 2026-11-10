# Troubleshooting dan batasan implementasi

[Kembali ke README](../README.md)

## 15. Troubleshooting

| Gejala                         | Pemeriksaan                                           |
| ------------------------------ | ----------------------------------------------------- |
| `secrets.h` tidak ditemukan    | Salin template ke lokasi yang benar                   |
| Ethernet hardware not found    | Daya, HSPI, CS, GND, dan modul W5500                  |
| Ethernet cable not connected   | Kabel, switch, dan LED link                           |
| IP tidak sama dengan fallback  | DHCP berhasil; gunakan IP aktual                      |
| Halaman OTA tidak terbuka      | Mode Ethernet, IP, routing LAN, TCP 80                |
| HTTP 401                       | Kredensial efektif di `Config.h`                      |
| HTTP 413                       | Ukuran binary dan tabel partisi                       |
| Firmware kembali ke versi lama | Reset/crash sebelum validasi selesai                  |
| Modbus `0xE2`                  | Alamat slave, catu PZEM, A/B, RX/TX, DE/RE, 9600 8N2  |
| Modbus `0xE3`                  | Noise, wiring, terminasi, dan konfigurasi serial      |
| Semua nilai nol                | Periksa log Modbus sebelum menyimpulkan listrik padam |
| MCB berubah sendiri            | Input GPIO 4 floating                                 |
| MQTT gagal                     | Broker, kredensial, port, dan Client ID unik          |
| `data/cmnd` tidak bereaksi     | Handler tindakan belum tersedia                       |
| LED ON tetapi MQTT offline     | LED hanya menunjukkan hasil komunikasi PZEM           |

Pada core Arduino ESP32 baru, nama class proyek `NetworkManager` dapat berbenturan dengan class bawaan. Jika terjadi error redefinition, gunakan versi platform yang kompatibel atau ubah nama class/header proyek beserta referensinya.

Versi platform `espressif32` saat ini belum dikunci di `platformio.ini`.

## 16. Batasan implementasi

- OTA Wi-Fi belum tersedia.
- Tidak ada fallback otomatis Ethernet/Wi-Fi.
- MQTT dan HTTP OTA belum memakai TLS.
- Belum ada verifikasi signature firmware khusus proyek.
- Data gagal baca dikirim sebagai nol tanpa status error Modbus pada JSON aktif.
- Belum ada antrean data offline.
- Hasil publish belum diperiksa.
- Reset energi belum memvalidasi respons PZEM.
- Upload dan beberapa operasi I/O menahan loop.
- DHCP lease belum dipelihara melalui `Ethernet.maintain()`.
- Rollback memeriksa kelangsungan eksekusi berbasis waktu.
- Folder test belum berisi pengujian otomatis aplikasi.

Dokumentasi ini disusun berdasarkan pembacaan kode. Verifikasi sensor, koneksi jaringan, upload OTA, dan rollback memerlukan pengujian pada perangkat nyata.
