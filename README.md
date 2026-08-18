# 🤖 esp32c3_super_mini_robot-bit-rxy — ESP32-C3 RISC-V Robot, bit-rxy BLE control

ESP32-C3 differential-drive robot, controlled over Bluetooth Low Energy by the free, unmodified **[bit-rxy](https://abourdim.github.io/bit-rxy/)** web app — joystick, D-Pad, horn, live speed/distance/battery gauges, and sound feedback, no paid app required. Firmware also supports **WiFi OTA updates** after the first USB flash.

This is a fork of [`esp32c3_super_mini_robot_remotexy`](https://github.com/abourdim/esp32c3_super_mini_robot_remotexy) (renamed from `esp32c3_super_mini_robot`; the original, still-untouched RemoteXY version) with the control layer replaced end to end. Everything else — servos, buzzer, NeoPixels, OLED, ultrasonic sensor, battery monitor — is unchanged. This repo is the sole working directory for all further robot firmware work.

📖 **Full write-up**: [wiki article](https://abourdim.github.io/wiki/wdiy-robot-en.html) — walkthrough, protocol internals, the joystick-flood post-mortem, and OTA setup, in more depth than this README.

## 🌐 Live site

**[abourdim.github.io/esp32c3_super_mini_robot-bit-rxy](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/)** — auto-deployed from `01_software/01_app/02_web/` on every push to `main`.

> **If the live site 404s on a page that clearly exists in `02_web/`**, even though `pages.yml` reports success: check the repo's Pages source setting (`gh api repos/abourdim/esp32c3_super_mini_robot-bit-rxy/pages --jq '.build_type'`). It must be `workflow` (GitHub Actions), not `legacy` (deploy from a branch) — `pages.yml` uses the modern `actions/deploy-pages` method, which only takes effect if Pages is configured to receive it. If it's wrong: `gh api -X PUT repos/abourdim/esp32c3_super_mini_robot-bit-rxy/pages -f build_type=workflow`, then re-run the workflow.

### 📚 Guides — by audience

| Guide | For | What it covers |
|---|---|---|
| [`user-guide.html`](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/user-guide.html) | 🦸 Kids · end users | Power on · pair phone · drive · light/sound meanings · troubleshooting · safety |
| [`build-guide.html`](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/build-guide.html) | 🔧 Makers | BOM (~$30–45) · tools · wiring · step-by-step assembly · 3D parts · first flash |
| [`start-here.html`](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/start-here.html) | 💻 Developers | Firmware narrative tour |
| [`learn.html`](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/learn.html) | 🧠 Curious devs | Reference manual — `launch.sh` options, `platformio.ini` directives, RMT, partition table |
| [`hardware.html`](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/hardware.html) | ⚡ Hardware hackers | Live PCB browser, GPIO pin map, links to KiCad source + gerbers + interactive BOM |
| [`instructor.html`](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/instructor.html) | 🧑‍🏫 Teachers | Prereqs · lesson plan · student FAQ · cheat sheet |

These guides are inherited from the original repo and describe the general chassis/build/hardware — they predate the bit-rxy/BLE conversion, so anything about *controlling* the robot (RemoteXY-specific screenshots, etc.) is stale. Driving, BLE, and OTA are covered by this README and the wiki article instead.

### 🛠️ Tools — runs in your browser

| Tool | What it does |
|---|---|
| [`flash.html`](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/flash.html) | Click-to-flash via Web Serial (Chrome/Edge) |
| [`monitor.html`](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/monitor.html) | Live serial monitor in the browser |
| [`audit.html`](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/audit.html) | Bug tracker · severity dashboard |
| [`index.html`](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/) | Launcher landing page |

## 🚀 Quick start

```bash
git clone https://github.com/abourdim/esp32c3_super_mini_robot-bit-rxy.git
cd esp32c3_super_mini_robot-bit-rxy/01_software/01_app
./launch.sh flash              # first flash: compile + upload over USB + monitor
```

Then open **[abourdim.github.io/bit-rxy](https://abourdim.github.io/bit-rxy/)**, tap **Connect**, and pick `diy_app_b3` (or a `uBit`-prefixed name, depending on firmware revision) from the device chooser. The app auto-loads the robot's layout — joystick, D-Pad, horn, gauges, battery, sound — and switches to Play mode.

## 📡 WiFi OTA updates

After the first USB flash, every update after that can go over WiFi instead. Hold the debug button (GPIO0, same one used as an in-game action button):

| Hold duration | Result |
|---|---|
| ~3 seconds | Enters OTA mode, reconnects to the last WiFi network it joined |
| ~8 seconds (keep holding) | Forgets the saved network and opens a **`WDIY-Robot-Setup`** captive portal to pick a new one |

No SSID/password compiled in — [WiFiManager](https://github.com/tzapu/WiFiManager) handles storage and the setup portal. Once the OLED shows `OTA ready` and an IP:

```bash
cd 01_software/01_app
./ota_flash.sh <robot-ip>                       # build from source, then flash
./ota_flash.sh <robot-ip> path/to/firmware.bin   # flash an exact pre-built .bin instead
```

Full walkthrough (captive portal screenshots, troubleshooting): [wiki article, OTA section](https://abourdim.github.io/wiki/wdiy-robot-en.html#ota).

**Why two `.pio/build/` folders?** `platformio.ini` defines two environments — `esp32-c3-devkitm-1` (USB, `upload_protocol = esptool`) and `esp32-c3-devkitm-1-ota` (WiFi, `upload_protocol = espota`, no serial-specific `upload_flags`). PlatformIO always gives each environment its own isolated build cache, even when one `extends` the other and only overrides a couple of settings — that's normal, not a sign anything's wrong.

## 📁 Structure

```
esp32c3_super_mini_robot-bit-rxy/
├── 01_software/
│   ├── 01_app/                    ⚙ main robot firmware (ESP32-C3 Arduino/PlatformIO)
│   │   ├── 01_src/                source files (one .h/.cpp pair per system)
│   │   │   ├── 03_bit-rxy.cpp/.h  BLE control layer — speaks bit-rxy's protocol
│   │   │   ├── 04_tasks.cpp/.h    robot logic (joystick → servos, sensors → gauges)
│   │   │   └── 17_ota.cpp/.h      WiFi OTA (WiFiManager + ArduinoOTA)
│   │   ├── 02_web/                browser tools (canonical Pages source, inherited)
│   │   ├── platformio.ini         build orders — main env + esp32-c3-devkitm-1-ota env
│   │   ├── launch.sh              interactive launcher menu (USB flash/monitor)
│   │   ├── layout_*.json          the 8 panels the robot serves (source of truth)
│   │   ├── layout_cfg.sh          decode/encode a layout JSON ↔ base64
│   │   ├── install_export.py      repair + slim an Arrange export, splice it in
│   │   ├── gen_test_layouts.py    regenerate the six test-mode panels
│   │   └── ota_flash.sh           push firmware over WiFi OTA
│   ├── 02_calibration/            calibration sketch
│   ├── 03_demo/                   demo project
│   ├── 06_tests/                  standalone bring-up sketches (no BLE, no app)
│   └── 4_wled/                    vendored WLED source (unrelated sub-project)
├── 02_hardware/                   PCB design (KiCad, v1–v3)
└── 03_3d/                         3D-print files
```

## 🎛 Panels

The robot owns its layout: the app is a generic renderer and draws whatever CFG
arrives on connect. Eight panels are compiled in, chosen from the **Level**
selector, which persists in NVS and re-sends the layout live — the panel
redraws without a reconnect and without reflashing.

| mode | what it is |
|---|---|
| **Beginner** | drive, two gauges, battery — 9 controls |
| **Expert** | everything: drive, distance + graph, lights, sound, system — 30 controls |
| Motors / Distance / Lights / Sound / Display / Power | one subsystem each, for classroom bring-up |

The test panels exist so a child moves between exercises with a dropdown
instead of an adult reflashing between them. Each carries the Level selector
itself — a panel without it would strand the robot until someone reflashed.

Two tests stay deliberately **outside** this system, in `01_software/06_tests/`:
`test_leds` and `test_ble` have no radio and no layout at all. An app-driven
test cannot distinguish a dead subsystem from a failed connection; those two
answer *is it powered* and *does the radio work* first, so everything
downstream can assume both.

Telemetry is sent **on change**, with per-value deadbands, and gated on which
panel is active — publishing to widget ids the current layout does not contain
is a notification into nothing, on a radio this firmware works hard to keep out
of `rc=6`.

## 🔌 Two apps, two dialects

This robot is driven by both stock [bit-rxy](https://abourdim.github.io/bit-rxy/)
and the richer keystudio app, and they disagree about the D-pad and the
joystick. Both encodings of each are accepted, with bit-rxy's handled exactly
as before:

```
D-pad     SET <id> <up|down|left|right> <0|1>   bit-rxy
          <a..p>                                keystudio, 1-byte state mask
          M <0..15>                             text form of that mask
joystick  SET <id> <angle> <distance>           bit-rxy
          SET <id> <x> <y>                      keystudio
```

The joystick pair is genuinely ambiguous inside the 0..100 quadrant, so it is
resolved strongest-evidence-first — see `handleJoystick()`.

`sendCfg()` sizes its chunks to the negotiated MTU rather than the rxy MakeCode
template's fixed 18, and announces the count as `CFGBEGIN <n>` so a client can
show a real progress bar. At 18 the expert layout would be ~19s per connect.

## 🎨 Editing a layout

Arrange the panel in the app, export the Layout JSON, then:

```bash
cd 01_software/01_app
python3 install_export.py ~/Downloads/wdiy-robot-b3-layout-*.json
pio run -t upload
```

It repairs what the export gets wrong (group headers colliding with the first
widget inside, a couple of pixels of overlap from a stray drag), drops the
~90 fields the app re-creates on load, validates, and splices the result into
the firmware. It refuses to install on any validation failure rather than
producing a broken panel.

## 🔧 Hardware map

From `01_software/01_app/01_src/00_config.h` — the single place these are defined.

| Function | GPIO | Notes |
|---|---|---|
| Left drive servo | 6 | continuous rotation, 90 = stop |
| Right drive servo | 3 | mounted mirrored, so its pulse range is inverted |
| Red LED | 10 | link status **and** app-controllable — see below |
| Green LED | 1 | link status **and** app-controllable |
| Buzzer | **4** | shares its pin with battery sense |
| Battery sense (ADC) | **4** | shares its pin with the buzzer |
| NeoPixel strip | 5 | 4 LEDs |
| Ultrasonic trig / echo | 21 / 20 | HC-SR04 |
| OLED SDA / SCL | 8 / 9 | SSD1306 128×64 at 0x3C |
| Button | 0 | also the OTA entry: hold 3 s |

BLE device name: `diy_app_b3`. Battery scale: 3.3 V empty → 4.5 V full.

> **GPIO 4 is used twice.** `CONFIG_PIN_BUZZER` and `CONFIG_PIN_BATTERY_LEVEL`
> are the same pin. This predates the current firmware and has not been
> rewired, but the **Power** test panel deliberately puts a Buzz button next to
> the live voltage reading so the interaction is visible rather than hidden.
> If the volts jump while buzzing, that is the cause.

**The two board LEDs are the link indicator by default** — green blinks while
connected, red while not. The app does not own them until the first toggle;
`onDisconnect()` hands them back, so "not connected" stays visible even if a
user left both switched off. The panel's status dots report *which LED is
active*, not the blink phase — publishing the raw pin would flip the indicator
every telemetry cycle and defeat the send-on-change gate.

## 🕹 Widgets

Everything the firmware answers to. Ids are stable; the layouts are just
different subsets of them.

**Controls (app → robot)**

| id | does |
|---|---|
| `joy_drive` / `dpad_drive` | both write the same steering state, whichever was touched last |
| `spd` | scales the drive mix, so steering geometry survives a low ceiling |
| `btn_stop` | clears every input that could re-assert motion, then stops the servos |
| `btn_horn` | horn / alarm |
| `btn_buzz` | one beep per press |
| `toggle_led_r` / `toggle_led_g` | take the board LEDs over from the status indicator |
| `toggle_np` | strip on/off |
| `np_r` / `np_g` / `np_b` | one colour channel each, recombined into 0xRRGGBB |
| `np_effect` | Solid, Rainbow, Knight Rider, Duel eye, French flag |
| `np_bright` | FastLED brightness |
| `oled_text` | text typed here becomes the OLED's top line |
| `upd` | telemetry verbosity: Off / Basic / All |
| `level` | which panel the robot serves |

**Readouts (robot → app)**

| id | source |
|---|---|
| `gauge_speed` | drive magnitude 0–100 |
| `gauge_distance` | ultrasonic, 0–200 cm |
| `graph_dist` | the same measurement as a time series |
| `alert` | obstacle toast, under 30 cm, re-arms above 40 |
| `battery_level` / `lbl_vbat` | percentage and raw pack voltage |
| `gauge_rssi` | link strength via `ble_gap_conn_rssi()` |
| `led_button` | the board's push button |
| `led_r_state` / `led_g_state` | which board LED is active |
| `lbl_ver` / `lbl_uptime` | firmware version and uptime |
| `lbl_oled` | what the OLED's top line actually reads |
| `sound_alert` | plays a tone on the phone |

Only `np_effect` still couples a string between layout and firmware; the colour
is three numbers precisely so there is no name table to keep in sync.

## 📶 Telemetry

Sent **on change**, never on a timer, with a deadband per value so sensor noise
cannot manufacture updates: 1 cm on distance, 1 % on speed and battery, 0.05 V
on the pack, 2 dBm on RSSI, and a whole second on uptime. `lbl_ver` goes once
per CFG. A parked robot on the Expert panel drops from 11 notifications per
cycle to roughly two.

`graph_dist` is the deliberate exception and is sent every cycle: a graph is a
time series, and suppressing repeats would compress the flat stretches and
distort the trace rather than just saving traffic.

Two mechanisms keep it honest:

- **A forced refresh after every CFG transfer.** A freshly rendered panel has
  no values in it, so a widget whose reading had not changed would sit blank.
- **Gating on the active panel.** Publishing to widget ids the current layout
  does not contain is a notification into nothing; the Sound panel sends none
  at all.

This matters because this radio has a documented history of drowning in `rc=6`
(`BLE_HS_ENOMEM`) when `notify()` is called too freely — the bug that shaped
the whole bit-rxy conversion.

## 🧪 Bring-up tests

`01_software/06_tests/` holds two sketches that are deliberately **outside**
the panel system — no radio, no layout, no app:

| sketch | answers |
|---|---|
| `test_leds` | is the board powered, and am I flashing what I think I am? |
| `test_ble` | does the radio advertise and accept a connection? |

Run them in that order first. An app-driven test cannot distinguish a dead
subsystem from a failed connection, and neither can a child — so these two
settle both questions before anything downstream is believed. `test_ble`
blinks slowly while advertising and goes solid on connect, readable across a
room with no serial monitor.

The six **test panels** in the Level selector cover the rest, and need no
reflashing between them.

## 🐛 Known issues

See [`audit.html`](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/audit.html) for the live tracker (inherited dashboard, predates the bit-rxy conversion — treat entries about RemoteXY as historical). Still-relevant platform-level issues:
- **RMT ISR recursion crash on ESP32-C3** — fixed by pinning to `espressif32 @ 6.7.0` (ESP-IDF 5.1) in `platformio.ini`.
- **Native USB-CDC upload quirks** — mid-upload baud switch and stub hand-off both crash the C3's native USB endpoint; fixed via `upload_speed = 115200` + `upload_flags = --no-stub`.
- The joystick-flood BLE bug (`rc=6`/`BLE_HS_ENOMEM`) that shaped the bit-rxy conversion is documented in full in the [wiki article's post-mortem section](https://abourdim.github.io/wiki/wdiy-robot-en.html#postmortem).

**Current limitations, stated plainly:**
- `GETCFGVER` is recognised but not answered. Replying means reproducing the
  app's revision hash exactly, and a mismatch fails silently into permanent
  cache misses, so every connect re-sends the whole layout. On a low negotiated
  MTU that is several seconds.
- The joystick encoding is ambiguous inside the 0–100 quadrant when the app
  skips `GETCFGVER` (its hard-refresh path). A drag confined to up-and-right
  reads as angle/distance until a negative value proves otherwise.
- `gauge_speed` and `gauge_distance` sit under ~175 px in the current expert
  layout and may clip their unit labels. Both were sized by hand.
- Flash is at ~90 %. Each new panel costs roughly 1 KB between firmware and its
  embedded blob.

## 📜 License

Vendored WLED is MIT — see `01_software/4_wled/WLED/LICENSE`. Project code: license TBD.
