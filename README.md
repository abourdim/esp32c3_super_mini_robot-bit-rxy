# 🤖 esp32c3_super_mini_robot — ESP32-C3 RISC-V Robot

ESP32-C3 robot project — Arduino/PlatformIO firmware, browser-based flasher and monitor, audit dashboard, and a 9-theme Workshop-DIY web UI.

## 🌐 Live site

**[abourdim.github.io/esp32c3_super_mini_robot](https://abourdim.github.io/esp32c3_super_mini_robot/)** — auto-deployed from `01_software/01_app/02_web/` on every push.

### 📚 Guides — by audience

| Guide | For | What it covers |
|---|---|---|
| [`user-guide.html`](https://abourdim.github.io/esp32c3_super_mini_robot/user-guide.html) | 🦸 Kids · end users | Power on · pair phone · drive · light/sound meanings · troubleshooting · safety |
| [`build-guide.html`](https://abourdim.github.io/esp32c3_super_mini_robot/build-guide.html) | 🔧 Makers | BOM (~$30–45) · tools · wiring · step-by-step assembly · 3D parts · first flash |
| [`start-here.html`](https://abourdim.github.io/esp32c3_super_mini_robot/start-here.html) | 💻 Developers | Firmware narrative tour. 14 missions: layout, build, flash, debug. |
| [`learn.html`](https://abourdim.github.io/esp32c3_super_mini_robot/learn.html) | 🧠 Curious devs | Reference manual. Every `launch.sh` option · `platformio.ini` directive · RMT · partition table · addr2line · Web Serial. |
| [`hardware.html`](https://abourdim.github.io/esp32c3_super_mini_robot/hardware.html) | ⚡ Hardware hackers | Live PCB browser. 5-layer toggle · GPIO pin map · connector reference · links to KiCad source + gerbers + interactive BOM. |
| [`instructor.html`](https://abourdim.github.io/esp32c3_super_mini_robot/instructor.html) | 🧑‍🏫 Teachers | Prereqs · 90-min lesson plan · student FAQ · cheat sheet · reset checklist |

### 🛠️ Tools — runs in your browser

| Tool | What it does |
|---|---|
| [`flash.html`](https://abourdim.github.io/esp32c3_super_mini_robot/flash.html) | Click-to-flash via Web Serial (Chrome/Edge) |
| [`monitor.html`](https://abourdim.github.io/esp32c3_super_mini_robot/monitor.html) | Live serial monitor in the browser |
| [`audit.html`](https://abourdim.github.io/esp32c3_super_mini_robot/audit.html) | Bug tracker · severity dashboard · BUG-NNN forensics |
| [`index.html`](https://abourdim.github.io/esp32c3_super_mini_robot/) | Launcher landing page |

## 📁 Structure

```
esp32c3_super_mini_robot/
├── 01_software/
│   ├── 01_app/                  ⚙ main robot firmware (ESP32-C3 Arduino)
│   │   ├── 01_src/              source files (one .h/.cpp pair per system)
│   │   ├── 02_web/              browser tools (canonical Pages source)
│   │   ├── platformio.ini       build orders (pinned platform & libs)
│   │   └── launch.sh            interactive launcher menu
│   ├── 02_calibration/          calibration sketch
│   ├── 03_demo/                 demo project
│   ├── 05_bare_minimum/         minimal sketch
│   └── 4_wled/                  vendored WLED source
├── 02_hardware/                 (PCB design — TBD)
└── 03_3d/                       (3D-print files — TBD)
```

## 🚀 Quick start

```bash
cd 01_software/01_app
./launch.sh                    # interactive menu (option 0 = full setup)
./launch.sh flash              # one-shot: compile + upload + monitor
```

Everything else is documented inside the launcher menu and in the live site's [Start Here](https://abourdim.github.io/esp32c3_super_mini_robot/start-here.html) guide.

## 🎨 Themes

The web tools ship with **9 themes in 4 audience-mood groups**:

- 🔬 **Science** — `lab-light` · `lab-dark`
- 💻 **Hacker** — `terminal` · `solarized`
- 🤖 **Pro** — `robot` (default)
- 🦸 **Kids action** — `bot-pop` · `kapow` · `blastoff` · `dino`

Switch in **Settings (⚙️)** in any tool. Plus 3 languages (EN/FR/AR with RTL).

## 🐛 Known bugs

See [`audit.html`](https://abourdim.github.io/esp32c3_super_mini_robot/audit.html) for the live tracker. Headline incidents documented:
- **BUG-001** — RMT ISR recursion crash on ESP32-C3 (✅ fixed by pinning to ESP-IDF 5.1)
- **BUG-002** — Ctrl+C in MSYS2 monitor (🔍 open)
- **BUG-003** — Unpinned platform → non-reproducible builds (✅ fixed)
- **BUG-004** — RemoteXY "No PRO license" (🔍 open — UI exceeds free 5-var quota)
- **BUG-005** — Native USB CDC reset race during esptool upload (🔍 open)

## 📜 License

Vendored WLED is MIT — see `01_software/4_wled/WLED/LICENSE`. Project code: license TBD.
