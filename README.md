# lanzones-robotics-os

Custom STM32 firmware for Team Lanzones' competition robots — **TALON** (sumobot)
and **VECTOR** (line follower), both with a full OLED/button menu OS for
on-the-fly tuning, strategy building, and diagnostics.
**Partnered with Koogs Robotics.**

## Projects

| Folder | OS | Robot | Highlights |
|---|---|---|---|
| [`TALON/`](TALON/) | TALON | Sumobot | Strategy Builder (phased playbooks), 5x ToF + edge sensors, IMU flip detection, give-up safety timer |
| [`VECTOR/`](VECTOR/) | VECTOR | Line follower | Path Array Editor (F/L/R/U), Learn Mode + Speed Run, 8x IR bar-graph, auto-cal wizard |
| [`shared/`](shared/) | — | — | Shared OLED/button UI framework, storage, buzzer, battery, watchdog |

Each robot runs exactly one OS; the two PlatformIO projects are built and
flashed independently. Both target the WeAct Black Pill V3.0 (STM32F401CE,
Arduino framework).

## Shared OS-layer hardware (both robots)

128x64 I2C OLED (PB6/PB7), 5 buttons (UP/DOWN/SELECT/BACK on PB12–PB15,
START/STOP on PA0), buzzer on PA1, status LEDs on PB8 (green) / PB9 (red),
battery sense on PB0. See `shared/LanzonesOS/src/LzPins.h` for the authoritative
table. Project-specific pins (motors, sensors, encoders) live in each project's
`include/pin_config.h` and are **placeholders until the custom PCB wiring is
final** — every unconfirmed pin is marked there.

## Build & upload

```
cd TALON            # or VECTOR
pio run             # build
pio run -t upload   # flash (board in DFU mode: hold BOOT0, tap NRST, release)
```

Uploading uses STM32CubeProgrammer CLI (`upload_protocol = custom`) because
`upload_protocol = dfu` (dfu-util) hits a known `LIBUSB_ERROR_PIPE` bootloader
bug on this chip, and PlatformIO has no native `cubeprogrammer` protocol — it
silently ignores that value. STM32CubeProgrammer must be installed at its
default path (see each `platformio.ini`).

## License

Proprietary / All rights reserved — see [LICENSE](LICENSE).
Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics.
