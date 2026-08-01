# Wheel calibration sketch

Finds each wheel's true stop point on continuous-rotation servos. Same BLE/OLED/OTA plumbing as `01_app`, but the widgets drive raw servo pulses directly (0-180) instead of the arcade-mixed joystick/D-Pad — no guessing which combined stick position corresponds to a given pulse.

## Flash it

```bash
cd 01_software/02_calibration
./launch.sh flash
```

## Use it

Open [bit-rxy](https://abourdim.github.io/bit-rxy/), connect to `diy_calib_b3`. Layout:

- **Left / Right sliders** — drag for coarse adjustment (0-180).
- **Left # / Right # edit fields** — type an exact number for fine adjustment; slider and edit field for the same wheel share state, whichever you touched last wins.
- **Center button** — resets both wheels to 90 (stop).

The OLED shows the live pulse value for each wheel as you adjust. Watch the actual wheel: find the pulse value where it visibly stops turning, note it down, and put it in `01_app/01_src/00_config.h` as `CONFIG_SERVO_SPEED_STOP_LEFT`/`_RIGHT` (plus whatever small `_OFFSET` correction gets it dead-centered).

## OTA

Same as `01_app` — hold the debug button ~3s to reconnect to the last WiFi network, ~8s to forget it and open the `WDIY-Calib-Setup` portal. See the [wiki article's OTA section](https://abourdim.github.io/wiki/wdiy-robot-en.html#ota) for the full walkthrough (applies identically here, just a different AP/hostname).

```bash
./ota_flash.sh <robot-ip>
```
