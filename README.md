# LilyGo EPD47 Bike Computer

A standalone bike-computer dashboard for the LilyGo T5 4.7-inch e-paper S3.
It renders locally with the
[LilyGo EPD47 `esp32s3` driver](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47/tree/esp32s3)
and refreshes every 10 seconds. Wi-Fi is used briefly at startup to synchronize
the RTC from NTP; no TRMNL API or deep-sleep refresh is used.

## Dashboard

- Large speed and trip-distance fields reserved for a future UART GPS
- PCF8563 RTC time
- MS5611 temperature, pressure, and estimated altitude
- BMI270 incline
- Battery voltage

The future GPS integration point is in `readBikeComputerData()` in `src/main.cpp`.
Populate `speedKph`, `distanceKm`, and `gpsValid` from the UART GPS parser.

## Hardware

| Device | I2C address |
| --- | --- |
| PCF8563 RTC | `0x51` |
| GT911 touch | `0x5D` |
| BMI270 IMU | `0x69` |
| MS5611 barometer | `0x77` |

I2C uses SDA GPIO18 and SCL GPIO17. More details are in
[docs/hardware-i2c.md](docs/hardware-i2c.md).

## Build And Flash

```sh
pio run
pio run --target upload
pio device monitor --environment T5-ePaper-S3
```

The configured upload and monitor port is `/dev/ttyACM1`.

Create `src/secrets.hpp` from `src/secrets.example.hpp` and enter the Wi-Fi
credentials. `src/secrets.hpp` is ignored by git. NTP server, timeout, and
timezone are configured in `src/config.hpp`.

To manually set the RTC, edit the `RTC_SYNC_*` fields in `src/config.hpp`, set
`RTC_MANUAL_SYNC_ENABLED` to `1`, flash once, then set it back to `0`.
