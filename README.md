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
│   │   ├── layout_cfg.sh          decode/encode the bit-rxy layout JSON ↔ base64
│   │   └── ota_flash.sh           push firmware over WiFi OTA
│   ├── 02_calibration/            calibration sketch
│   ├── 03_demo/                   demo project
│   └── 4_wled/                    vendored WLED source (unrelated sub-project)
├── 02_hardware/                   PCB design (KiCad, v1–v3)
└── 03_3d/                         3D-print files
```

## 🐛 Known issues

See [`audit.html`](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/audit.html) for the live tracker (inherited dashboard, predates the bit-rxy conversion — treat entries about RemoteXY as historical). Still-relevant platform-level issues:
- **RMT ISR recursion crash on ESP32-C3** — fixed by pinning to `espressif32 @ 6.7.0` (ESP-IDF 5.1) in `platformio.ini`.
- **Native USB-CDC upload quirks** — mid-upload baud switch and stub hand-off both crash the C3's native USB endpoint; fixed via `upload_speed = 115200` + `upload_flags = --no-stub`.
- The joystick-flood BLE bug (`rc=6`/`BLE_HS_ENOMEM`) that shaped the bit-rxy conversion is documented in full in the [wiki article's post-mortem section](https://abourdim.github.io/wiki/wdiy-robot-en.html#postmortem).

## 📜 License

Vendored WLED is MIT — see `01_software/4_wled/WLED/LICENSE`. Project code: license TBD.
