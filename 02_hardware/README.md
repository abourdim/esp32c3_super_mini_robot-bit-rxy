# 🔌 Hardware

KiCad source, PCB renders, BOMs, and gerbers for the Robot-01 board.

## Versions

| Version | Date | Status | Folder |
|---|---|---|---|
| **v3** | June–July 2025 | ✅ **Current — schematic source of truth** | [`v3/`](v3/) |
| v2 | May–June 2025 | 🔒 Archived | [`v2/`](v2/) |
| v1 | Aug 2025 (1st batch) | 🔒 Archived | [`v1/`](v1/) |

## Live browser

The current version is also rendered as a layered SVG viewer on the website:
**[abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/hardware.html](https://abourdim.github.io/esp32c3_super_mini_robot-bit-rxy/hardware.html)** — toggle layers, zoom, link to source.

## v3 contents

```
v3/
├── 37_rich_light_move_v3.kicad_sch      ← schematic (S-expressions, text)
├── 37_rich_light_move_v3.kicad_pcb      ← PCB layout
├── 37_rich_light_move_v3.kicad_pro      ← KiCad project file
├── 37_rich_light_move_v3-F_Cu.svg       ← front copper render
├── 37_rich_light_move_v3-B_Cu.svg       ← back copper render
├── 37_rich_light_move_v3-F_Silkscreen.svg
├── 37_rich_light_move_v3-F_Courtyard.svg
├── 37_rich_light_move_v3-Edge_Cuts.svg
├── 00_lcsc/                             ← LCSC parts BOMs
├── bom/ibom.html                        ← interactive BOM (KiCad InteractiveHtmlBom)
└── production/                          ← gerbers + CSVs for fab
    ├── 37_richa_light_move_v3.zip       ← gerbers (send to PCBWay/JLCPCB)
    ├── bom.csv
    ├── designators.csv
    ├── netlist.ipc
    └── positions.csv
```

## Top-level BOM files

`bom_lcsc.xls`, `bom_jlcpcb2.xls`, `bom_assembly_jlcpcb_21062025.xls` — alternate BOM exports for different fab houses.

## Auto-backups (gitignored)

KiCad creates `*-backups/` folders with timestamped zips on every save.
These are **not tracked** — see project root `.gitignore`. Real source is in
the `.kicad_sch` / `.kicad_pcb` files themselves.

## Datasheet

`2410121311_S-S-SFN-1407PA7-6_C360603.pdf` — datasheet for one of the
LCSC parts used in the build (component lookup by C-number).
