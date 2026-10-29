# Lab 2 — Seven Segment Display (ESP32 + ESP-IDF + Wokwi)

Five self-contained ESP-IDF projects for the ESP32 DevKit-C V4. Every folder
has its own `main/main.c`, `diagram.json`, `wokwi.toml`, and a **pre-built**
`build/` directory that Wokwi can flash directly.

| Folder | Lab steps | Function | Submission check |
|---|---|---|---|
| `task_1_to_4_auto_count_0_9` | 1–4 | Auto count 0→9, 1 s per digit | — |
| `task_5_to_8_switch_count_0_9` | 5–8 | Push button counts 0→9 and wraps | — |
| `task_9_to_12_press_counter_max_9` | 9–12 | Press counter saturates at 9, reset button | **Check 1** |
| `task_13_dual_segment_88` | 13 | Static `88` on two digits | — |
| `task_14_to_17_counter_00_99` | 14–17 | Multiplexed 00–99 counter with buttons | **Check 2** |

Full laboratory report (Thai): [`LAB_REPORT.md`](LAB_REPORT.md)

Wiring reference: [`docs/WIRING.md`](docs/WIRING.md)

## Hardware and pin map

All projects use a **common-cathode** `wokwi-7segment` (segments active HIGH).
A 220 Ω resistor is placed in series with every segment.

| Signal | GPIO | Notes |
|---|---|---|
| A | 13 | top |
| B | 12 | top-right |
| C | 14 | bottom-right |
| D | 27 | bottom |
| E | 26 | bottom-left |
| F | 25 | top-left |
| G | 33 | middle |
| DIG1 / COM (tens / single digit) | 16 | active LOW common |
| DIG2 (units) | 17 | active LOW common |
| COUNT button | 18 | input, internal pull-up, active LOW |
| RESET button | 19 | input, internal pull-up, active LOW |

Single-digit projects connect `seg1:COM` to GND. Two-digit projects drive
`DIG1`/`DIG2` from GPIO16/GPIO17.

7-segment patterns (common cathode, bit0=A … bit6=G):

| Digit | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|---|
| Hex | 3F | 06 | 5B | 4F | 66 | 6D | 7D | 07 | 7F | 6F |

## Project layout

```
Lab2/
├── build_all.cmd              # double-click / cmd wrapper
├── build_all.ps1              # sets ESP-IDF env, builds all five projects
├── LAB_REPORT.md              # complete 17-step report
├── README.md
├── docs/
│   └── WIRING.md
└── task_1_to_4_auto_count_0_9/
    ├── CMakeLists.txt
    ├── sdkconfig.defaults
    ├── wokwi.toml
    ├── diagram.json
    ├── REPORT.md              # per-step report, flowchart and results
    ├── build/                 # generated, required by Wokwi
    └── main/
        ├── CMakeLists.txt
        └── main.c
```

The other four task folders use exactly the same structure.

## Build

The build script sets the ESP-IDF v6.1 environment itself, so it works even when
the PowerShell profile is blocked by execution policy.

```powershell
cd C:\microcontrollerPJ\Lab2
.\build_all.cmd
```

Manual build for one module:

```powershell
cd C:\microcontrollerPJ\Lab2\task_1_to_4_auto_count_0_9
idf.py set-target esp32     # only needed when sdkconfig is absent
idf.py build
```

`set-target` is required before the first build because Wokwi does not compile
the project. `idf.py build` then produces everything Wokwi reads:

```
build/flasher_args.json
build/bootloader/bootloader.bin
build/partition_table/partition-table.bin
build/<project>.bin
build/<project>.elf
```

### Verified build result (ESP-IDF v6.1, target esp32)

| Module | App `.bin` | Size | Status |
|---|---|---:|---|
| `task_1_to_4_auto_count_0_9` | `build/task_1_to_4_auto_count_0_9.bin` | 149,408 B (`0x247A0`) | OK |
| `task_5_to_8_switch_count_0_9` | `build/task_5_to_8_switch_count_0_9.bin` | 152,688 B (`0x25470`) | OK |
| `task_9_to_12_press_counter_max_9` | `build/task_9_to_12_press_counter_max_9.bin` | 153,024 B (`0x255C0`) | OK |
| `task_13_dual_segment_88` | `build/task_13_dual_segment_88.bin` | 149,424 B (`0x247B0`) | OK |
| `task_14_to_17_counter_00_99` | `build/task_14_to_17_counter_00_99.bin` | 153,120 B (`0x25620`) | OK |

Every module also contains `bootloader.bin` (26,176 B) and
`partition-table.bin` (3,072 B).

## Run in Wokwi

1. Open **one module folder** as the VS Code workspace, for example
   `C:\microcontrollerPJ\Lab2\task_14_to_17_counter_00_99`.
   Do not open the whole `Lab2` folder: Wokwi searches the workspace root for
   `diagram.json` and `wokwi.toml`.
2. Confirm `build/flasher_args.json` exists.
3. Run **Wokwi: Start Simulator**.
4. Open the serial monitor. Use the on-screen buttons, or the keyboard:
   - `b` = COUNT (GPIO18)
   - `r` = RESET (GPIO19)

`wokwi.toml` loads the complete flash image, not only the application:

```toml
[wokwi]
version = 1
firmware = 'build/flasher_args.json'
elf = 'build/task_14_to_17_counter_00_99.elf'
```

## Notes

- The three old modules from the previous lab (`module1_gpio_switch`,
  `module2_brightness_control`, `module3_seven_segment`) were removed because
  they are not part of the seven-segment 17-step assignment.
- The `build/` folders are intentionally kept so a fresh clone can start Wokwi
  without rebuilding. The `.gitignore` ignores the large intermediate tree but
  whitelists `flasher_args.json`, the application `.bin`/`.elf`, the bootloader
  and the partition table.
- A missing `build/flasher_args.json` is the usual cause of the Wokwi error
  `firmware binary build/flasher_args.json not found`: run the build script, or
  open the module folder itself as the workspace.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `firmware binary build/flasher_args.json not found` | `build/` missing or wrong workspace root | Run `build_all.cmd`, then open the module folder itself |
| `idf.py` not recognized | IDF environment not activated | Use `build_all.cmd`; it sets the environment |
| `running scripts is disabled` | PowerShell execution policy | `build_all.cmd` uses `-ExecutionPolicy Bypass` |
| Display blank | Common type mismatch | Keep `"common": "cathode"` in `diagram.json` |
| Ghost segments when counting | Missing blanking between digits | Keep `display_digits_off()` before changing segments |
| Counter jumps by two | Contact bounce | Keep the 50 ms `DEBOUNCE_US` window |
| Only one digit lights | DIG pin not driven LOW | Common cathode digits are active LOW |
