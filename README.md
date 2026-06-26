# LilyGo EPD47 Bike Computer

A standalone bike-computer dashboard for the LilyGo T5 4.7-inch e-paper S3.
It renders locally with the
[LilyGo EPD47 `esp32s3` driver](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47/tree/esp32s3)
and refreshes every 10 seconds. GPS is read over UART at 115200 baud and is used
for speed, coordinates, course, satellite status, and RTC synchronization. No
Wi-Fi, TRMNL API, or deep-sleep refresh is used.

## Dashboard

- Large speed, coordinate, course, and satellite fields from UART GPS
- GPS UTC time, with PCF8563 RTC fallback
- MS5611 temperature, pressure, and estimated altitude
- BMI270 incline
- Battery voltage

The GPS integration point is in `readBikeComputerData()` in `src/main.cpp`.
GPS parsing lives in `src/gps.cpp`.

## Hardware

| Device | I2C address |
| --- | --- |
| PCF8563 RTC | `0x51` |
| GT911 touch | `0x5D` |
| BMI270 IMU | `0x69` |
| MS5611 barometer | `0x77` |

I2C uses SDA GPIO18 and SCL GPIO17. More details are in
[docs/hardware-i2c.md](docs/hardware-i2c.md).

## GPS Wiring

The exposed GPIO45, GPIO10, GPIO48, and GPIO39 pins are free to use. The I2C
bus uses GPIO17 for SCL and GPIO18 for SDA.

For the UART GPS module with pins labeled `R`, `T`, `V`, and `G`:

| GPS pin | Meaning | LilyGo ESP32-S3 pin |
| --- | --- | --- |
| `R` | GPS RX | Not connected for receive-only use |
| `T` | GPS TX | GPIO48 / ESP RX |
| `V` | GPS power | 3V3 or 5V, depending on the GPS module |
| `G` | Ground | GND |

UART names are from each device's point of view: GPS `T` transmits data, so it
connects to the ESP32-S3 RX pin. GPS `R` receives data, but it is not needed for
the current receive-only firmware.

## Build And Flash

```sh
pio run
pio run --target upload
pio device monitor --environment T5-ePaper-S3
```

The configured upload and monitor port is `/dev/ttyACM1`.

To manually set the RTC, edit the `RTC_SYNC_*` fields in `src/config.hpp`, set
`RTC_MANUAL_SYNC_ENABLED` to `1`, flash once, then set it back to `0`.
