# lanzones-robotics-os

**Two robots. One brain design. Zero laptops in the pit.**

This is the custom STM32 firmware powering **Team Lanzones'** competition robots —
**TALON**, a sumobot that plans its fights, and **VECTOR**, a line follower that
memorizes mazes — built with **Koogs Robotics** as our hardware partner.

Every match-day decision — PID gains, attack playbooks, path corrections, speed
limits, calibration — happens **on the robot itself**, through a 128x64 OLED and
five buttons. Tune between bouts. Rebuild a strategy in the queue line. Clone a
whole configuration onto the backup unit over a serial cable. No recompiling, no
laptop, no excuses.

> Owned by Team Lanzones — Partnered by Koogs Robotics
> Proprietary / All rights reserved · [LICENSE](LICENSE)

---

## The two operating systems

| | **TALON** — Sumobot OS | **VECTOR** — Line Follower OS |
|---|---|---|
| Mission | Push the other robot out of the dohyo | Solve the maze, then speed-run it |
| Eyes | 5x VL53L1X laser ToF (wide-L/angled-L/front/angled-R/wide-R) + 2 boundary edge sensors + IMU | 8x IR reflectance array via CD4051 mux |
| Signature feature | **Strategy Builder** — named, phased battle playbooks | **Path Array Editor** — record, hand-edit, and replay F/L/R/U routes |
| Folder | [`TALON/`](TALON/) | [`VECTOR/`](VECTOR/) |

Each robot runs exactly one OS. Both are separate PlatformIO projects that share
one UI/OS layer ([`shared/LanzonesOS`](shared/LanzonesOS/)) — learn the menus on
one robot and you already know the other.

## What makes TALON interesting

A TALON **Strategy** is a named sequence of phases — SEARCH (Spin/Sweep/Crawl/
Charge, with a tunable differential-speed **Search Radius**), ATTACK (Ram/Curve/
Side-Slam, with launch **Ramp-Up**), RETREAT (with **Ramp-Down**), and precise
**Angled Turns** (15° steps, CW/CCW). Each phase ends on a trigger: elapsed time,
opponent detected, or edge detected. Six strategies can live in flash at once —
build `vs_HeavyBot` and `vs_FastBot`, then pick one in the queue.

Layered on top, because matches are chaos:

- **Edge Escape** — a strategy-level interrupt. Any edge trip, during *any*
  phase, abandons the current plan, runs your chosen escape maneuver, and
  resumes how you decided (restart / next phase / fall back to search). The
  only way to suppress it is deliberately, per-phase, time-boxed — via the
  **Sensor Ignore Window** (mute specific sensors for the first N ms of a
  phase, so a mid-rotation glitch can't fake a trigger).
- **Give-Up Timer** — an attack that gains no ground (no distance progress, no
  push impact) is a losing attack; the robot disengages instead of grinding.
- **Match Timer + Aggression Boost** — rulesets award ties to the "most
  aggressive" robot, so in the closing seconds TALON escalates to your
  designated maximum-commitment phase instead of waiting out the clock.
- **IMU auto-stop** (flipped robots stop spinning their wheels), **traction/
  slip detection** (encoders + IMU), a **5-second competition countdown**, a
  live match screen, and **Quick Rematch** — re-arm the next bout without
  touching a menu.
- **Safety interlock:** RUN MODE refuses to arm if the edge sensors are dead.
  A robot that can't see the boundary doesn't get to fight.

## What makes VECTOR interesting

**Learn Mode** drives the maze once (left-hand rule) and records every junction
as `F / L / R / U`. Got one wrong? Open the **Path Array Editor** and fix that
junction by hand — then **Speed Run** replays the corrected route without
re-deciding at intersections.

Micromouse-style, **every junction carries its own motion profile**: approach
speed for the straight leading in, brake time, Smooth-Arc vs Point-Turn
execution, post-turn speed, and a line-reacquisition timeout that stops the
robot with a `LOST LINE` error instead of letting it charge off blindly.
Unset fields fall back to the global speed profile.

The **finish line** (a perpendicular double-line, distinct from any junction)
stops the robot automatically — configurable per profile, and it ends Learn
Mode cleanly too. Plus: live 8-bar sensor visualization, a 3-second
self-sweeping auto-calibration wizard, and black-on-white / white-on-black
line modes.

## The shared OS layer

Both robots boot through a two-frame logo splash into the same UI system:

- **Every screen self-labels.** Breadcrumb + battery up top, four content rows,
  and a hint bar that always tells you what the five buttons do *right here*.
- **Named everything.** Profiles, strategies, and paths are named slots with
  Create / Open / Delete — destructive actions need a 1-second hold-to-confirm.
- **Lock Config** freezes all settings before a match (hold-gesture to unlock,
  survives power cycles — a bench reboot can't silently unlock your robot).
- **Config Export/Import over serial** — a tuned setup streams out as
  CRC-protected text and imports on another board. Torn or wrong-OS transfers
  are rejected before they touch anything.
- **Help is a manual, not a tooltip** — 15+ entries per OS, each with a worked
  example using real numbers.

## Engineering notes (the part we're proud of)

- **Non-blocking everywhere.** No `delay()` in the state machine; timed button
  debounce; sensor reads with short timeouts (a dead sensor shows FAIL — it
  never stalls the loop).
- **Motor control lives in a 200 Hz hardware-timer ISR** fed by a volatile
  sensor snapshot — menu navigation can never starve the control loop. Shared
  state uses `volatile` + brief critical sections.
- **The OLED redraws only what changed** — change-driven frames, 20 Hz cap,
  and dirty-region transfer that sends only modified 8-pixel tile rows.
- **Flash is sacred.** One CRC32-protected image in the last sector; writes
  happen only on explicit Save actions, are hard-blocked during RUN MODE, and
  edits batch in RAM (never a write per keypress).
- **An independent watchdog (8 s)** reboots a hung robot mid-match — because
  you can't press reset from outside the dohyo.
- **Pin-crunch solved properly:** the 48-pin STM32 ran out of GPIOs, so
  VECTOR's IR array runs through a CD4051 mux and TALON's ToF-XSHUT/edge
  lines through a PCF8574 expander with interrupt-driven edge reads.

## Hardware

WeAct Black Pill V3.0 (**STM32F401CE**), Arduino framework on PlatformIO.

Shared OS-layer pins (spec §1.2): OLED on I2C1 (PB6/PB7), buttons UP/DOWN/
SELECT/BACK on PB12–PB15, START/STOP on PA0, buzzer PA1, status LEDs PB8/PB9,
battery sense PB0. The authoritative table lives in
[`shared/LanzonesOS/src/LzPins.h`](shared/LanzonesOS/src/LzPins.h).

> ⚠️ **Project-specific pins are placeholders.** Motors, encoders, mux selects,
> expander address/INT, and the battery divider live in each project's
> `include/pin_config.h`, clearly marked `NOT FINAL` until the custom PCB
> layout lands. Update one file per project — no logic changes needed.

## Build & upload

```
cd TALON            # or VECTOR
pio run             # build
pio run -t upload   # flash — board in DFU mode first:
                    # hold BOOT0, tap NRST, release BOOT0
```

Uploading goes through **STM32CubeProgrammer CLI** (`upload_protocol = custom`),
because `dfu-util` hits a known `LIBUSB_ERROR_PIPE` bootloader bug on this chip
and PlatformIO has no native `cubeprogrammer` protocol (it silently ignores the
value — your build "succeeds" and nothing uploads). STM32CubeProgrammer must be
installed at the path set in each `platformio.ini`.

Serial console / config transfer: **USART1 on PA9/PA10 @ 115200**.

## Repository layout

```
lanzones-robotics-os/
├── TALON/                  # Sumobot OS (PlatformIO project)
│   ├── include/            #   pin_config.h, data model, build ID
│   └── src/                #   engine, screens, strategy builder, help
├── VECTOR/                 # Line Follower OS (PlatformIO project)
│   ├── include/
│   └── src/                #   engine, screens, path editor, help
└── shared/LanzonesOS/      # Shared OS layer (UI, input, storage, watchdog,
                            #   buzzer, battery, motors, serial transfer, logos)
```

## License & authorship

Copyright (c) 2026 **Team Lanzones**. Partnered by **Koogs Robotics**.
All rights reserved — this is a competition entry, not (yet) an open-source
project. Every source file carries the copyright header, and each firmware
embeds a documented build ID (`Help → LANZONES x KOOGS`) as an authorship
marker.
