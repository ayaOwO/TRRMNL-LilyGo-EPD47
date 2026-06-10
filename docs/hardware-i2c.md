# LilyGo T5 ePaper S3 Hardware and I2C Notes

This document records the hardware configuration and devices confirmed on the
tested LilyGo T5 ePaper S3 board.

## Board Configuration

- MCU: ESP32-S3
- Flash: 16 MB, QIO, 80 MHz
- PSRAM: 8 MB octal PSRAM
- Framework: ESP-IDF
- Native USB interface: USB Serial/JTAG
- Serial monitor speed: 115200 baud

The required ESP-IDF defaults are in `sdkconfig.defaults`. USB Serial/JTAG is
the primary console, and octal PSRAM is enabled.

## Serial Monitor

The board was detected as `/dev/ttyACM1` during testing. The exact ACM number
may change when other USB serial devices are connected.

```sh
pio device list
pio device monitor --environment T5-ePaper-S3
```

PlatformIO is configured with DTR and RTS inactive so opening the monitor does
not reset the ESP32-S3 into its ROM download bootloader.

If the monitor prints `waiting for download`, reset the board without holding
the boot button.

Only one process can own the serial port. Close an existing PlatformIO or
ESP-IDF monitor before uploading firmware.

## I2C Bus

Confirmed bus configuration:

| Signal | GPIO |
| --- | ---: |
| SDA | 18 |
| SCL | 17 |

- Clock: 50 kHz
- Idle line levels: SDA high, SCL high
- Internal pull-ups: enabled

The scanner must send an address-only I2C transaction. Passing a null buffer
and zero length to `i2c_master_write_to_device()` is rejected by ESP-IDF and
does not perform a valid scan.

## Detected Devices

| Address | Device | Confirmed result |
| --- | --- | --- |
| `0x51` | PCF8563-compatible RTC | Date and time registers read successfully |
| `0x5D` | GT911 touch controller | Product ID reports `911` |
| `0x69` | BMI270 IMU | Acceleration and gyroscope readings |
| `0x77` | MS5611 barometer | Temperature, pressure, and estimated altitude |

## Serial Output

The firmware scans the bus at startup and prints labeled readings once per
second.

```text
I2C devices found: 0x51, 0x5D, 0x69, 0x77

[RTC 0x51]
  Date: 2023-01-03
  Time: 11:03:34
  Weekday: 2
  Clock valid: yes

[Touch controller 0x5D]
  Active touches: 0
```

When the display is touched, each active touch prints its coordinates and
reported contact size:

```text
[Touch controller 0x5D]
  Active touches: 1
  Touch 1: X=123, Y=456, size=20
```

The relevant implementation is in `src/main.cpp`, `src/sensors.cpp`, and
`src/sensors.hpp`.
