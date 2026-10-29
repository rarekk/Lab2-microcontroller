# Wiring Reference — Lab 2

## 1. Single-digit projects (Task 1–12)

Components: ESP32 DevKit-C V4, `wokwi-7segment` (1 digit, common cathode),
7 × 220 Ω, 1–2 push buttons.

```
ESP32 GPIO13 ──[220R]── seg A ─┐
ESP32 GPIO12 ──[220R]── seg B  │
ESP32 GPIO14 ──[220R]── seg C  │
ESP32 GPIO27 ──[220R]── seg D  ├── 7-segment (common cathode)
ESP32 GPIO26 ──[220R]── seg E  │
ESP32 GPIO25 ──[220R]── seg F  │
ESP32 GPIO33 ──[220R]── seg G ─┘
ESP32 GND ───────────── seg COM

COUNT button : GPIO18 ──[button]── GND   (internal pull-up)
RESET button : GPIO19 ──[button]── GND   (Task 9–12 only)
```

| ESP32 pin | Wokwi part pin | Function |
|---|---|---|
| GPIO13 | `seg1:A` through `rA` | segment A |
| GPIO12 | `seg1:B` through `rB` | segment B |
| GPIO14 | `seg1:C` through `rC` | segment C |
| GPIO27 | `seg1:D` through `rD` | segment D |
| GPIO26 | `seg1:E` through `rE` | segment E |
| GPIO25 | `seg1:F` through `rF` | segment F |
| GPIO33 | `seg1:G` through `rG` | segment G |
| GND | `seg1:COM` | common cathode |
| GPIO18 | `btn_count:1.l` | count input |
| GND | `btn_count:2.l` | button return |
| GPIO19 | `btn_reset:1.l` | reset input |
| GND | `btn_reset:2.l` | button return |

## 2. Two-digit projects (Task 13–17)

```
ESP32 GPIO13..33 ──[220R each]── seg A..G ──┬── DIG1 (tens)  ← GPIO16
                                            └── DIG2 (units) ← GPIO17

DIG1 / DIG2 are common-cathode pins: LOW = digit enabled.
```

| ESP32 pin | Wokwi part pin | Function |
|---|---|---|
| GPIO16 | `seg1:DIG1` | tens common (active LOW) |
| GPIO17 | `seg1:DIG2` | units common (active LOW) |
| GPIO18 | `btn_count:1.l` | count input |
| GPIO19 | `btn_reset:1.l` | reset input |

No `COM` connection is used on the 2-digit part.

## 3. Electrical notes

- Common-cathode segments turn **ON** with a HIGH level.
- Common pins turn **ON** with a LOW level.
- 220 Ω per segment limits the current. In real hardware, multiplexing raises
  the instantaneous segment current; the simulation does not model this.
- GPIO18 and GPIO19 use internal pull-ups. The buttons connect to GND and are
  therefore active LOW; the firmware triggers on the falling edge.
- GPIO16/17 are outputs only in Task 13–17. GPIO16/17 are not used in the
  single-digit projects.

## 4. Wokwi part settings

```json
{ "type": "wokwi-7segment", "attrs": { "digits": "1", "common": "cathode", "color": "green" } }
{ "type": "wokwi-7segment", "attrs": { "digits": "2", "common": "cathode", "color": "green" } }
{ "type": "wokwi-pushbutton", "attrs": { "color": "red", "label": "COUNT (GPIO18)", "key": "b" } }
```

The board type must be `board-esp32-devkit-c-v4` (hyphen before `c`).
