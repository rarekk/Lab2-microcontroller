# Lab 2 - ESP32 GPIO, Interrupts, ADC, PWM and Seven-Segment (Wokwi)

Three self-contained ESP-IDF projects for the ESP32 DevKit-C V4. Each folder is
its own ESP-IDF project, with a Wokwi `diagram.json`, and a **pre-built**
`build/` directory containing `flasher_args.json` and all binaries Wokwi needs.

| Module | Topics | Input | Output |
|---|---|---|---|
| `module1_gpio_switch` | Digital I/O, GPIO interrupt, ISR debounce | Push button on GPIO4 | LED on GPIO2 |
| `module2_brightness_control` | ADC oneshot, LEDC PWM | Potentiometer on GPIO34 | LED on GPIO2 |
| `module3_seven_segment` | Multiplexed seven-segment display, button interrupt | Push button on GPIO18 | 2-digit display |

Build status (ESP-IDF **v6.1**, target `esp32`):

| Module | App binary | Size | Result |
|---|---|---:|---|
| `module1_gpio_switch` | `build/module1_gpio_switch.bin` | `0x253F0` (152,560 B) | Build OK |
| `module2_brightness_control` | `build/module2_brightness_control.bin` | `0x27A00` (162,304 B) | Build OK |
| `module3_seven_segment` | `build/module3_seven_segment.bin` | `0x25520` (152,864 B) | Build OK |

The `build/` folders are deliberately kept. Wokwi does **not** compile the
project, so deleting them causes `firmware binary build/flasher_args.json not
found`.

## Project layout

```
Lab2/
├── build_all.ps1
├── README.md
├── module1_gpio_switch/
│   ├── CMakeLists.txt
│   ├── sdkconfig
│   ├── sdkconfig.defaults
│   ├── wokwi.toml
│   ├── diagram.json
│   ├── build/                 # generated, required by Wokwi
│   └── main/
│       ├── CMakeLists.txt
│       └── main.c
├── module2_brightness_control/
│   └── ... same structure
└── module3_seven_segment/
    └── ... same structure
```

## Module 1 - GPIO switch with ISR debounce

- Button GPIO4: input with internal pull-up, active low, `GPIO_INTR_NEGEDGE`.
- The ISR is marked `IRAM_ATTR`, applies a **50 ms** time-window debounce with
  `esp_timer_get_time()`, then posts the GPIO number to a FreeRTOS queue.
- A worker task receives the event, toggles the LED and prints the press count.
- The LED blinks three times at startup to prove the output works.

Wiring:

| ESP32 pin | Component |
|---|---|
| GPIO4 | Push button `1.l` |
| GND | Push button `2.l` |
| GPIO2 | 220 ohm resistor -> LED anode |
| GND | LED cathode |

Expected serial output:

```
I (xxx) MODULE1: Ready: press the button on GPIO4 (LED on GPIO2).
I (xxx) MODULE1: Button GPIO4 pressed -> count=1, LED ON
I (xxx) MODULE1: Button GPIO4 pressed -> count=2, LED OFF
```

## Module 2 - Potentiometer ADC to PWM brightness

- Potentiometer wiper on **GPIO34 = ADC1 channel 6**.
- ADC oneshot driver, 12 dB attenuation, default 12-bit width.
- 16 samples are averaged per update (every 200 ms).
- Raw value `0..4095` is mapped to LEDC duty `0..255`.
- LEDC low-speed channel 0, 5 kHz, 8-bit resolution, LED on GPIO2.

Wiring:

| ESP32 pin | Component |
|---|---|
| 3V3 | Potentiometer `VCC` |
| GND | Potentiometer `GND` |
| GPIO34 | Potentiometer `SIG` |
| GPIO2 | 220 ohm resistor -> LED anode |
| GND | LED cathode |

Expected serial output:

```
I (xxx) MODULE2: ADC raw=2048 (1650 mV) -> duty=127/255 (49%)
```

Turn the knob with the mouse, or use Left/Right, Page Up/Page Down, Home/End
after clicking the potentiometer.

## Module 3 - Multiplexed 2-digit seven-segment display

- Wokwi `wokwi-7segment` with `"digits": "2"` and `"common": "cathode"`.
- Segments A-G: GPIO13, GPIO12, GPIO14, GPIO27, GPIO26, GPIO25, GPIO33.
- `DIG1` (tens) = GPIO16, `DIG2` (units) = GPIO17, both active low.
- Only one digit is enabled at a time; the firmware alternates every 2 ms
  (about 250 Hz full refresh) to create the multiplexed display.
- Button GPIO18 increments a `00..99` counter. The same 50 ms ISR debounce and
  queue pattern as Module 1 is used.
- Startup self-test shows `8` on each digit for 400 ms.

Wiring:

| ESP32 pin | Component |
|---|---|
| GPIO13/12/14/27/26/25/33 | 220 ohm resistor -> segments A/B/C/D/E/F/G |
| GPIO16 | Display `DIG1` (tens common) |
| GPIO17 | Display `DIG2` (units common) |
| GPIO18 | Push button `1.l` |
| GND | Push button `2.l` |

Expected serial output:

```
I (xxx) MODULE3: Ready: press GPIO18 to count. Display is multiplexed at ~250 Hz.
I (xxx) MODULE3: Button GPIO18 pressed -> count=01
I (xxx) MODULE3: Button GPIO18 pressed -> count=02
```

## Build

Open the **ESP-IDF PowerShell** (or run the activation script), then build one
module:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"

cd C:\microcontrollerPJ\Lab2\module1_gpio_switch
idf.py set-target esp32
idf.py build
```

Repeat for `module2_brightness_control` and `module3_seven_segment`.

To rebuild all three modules with one command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\microcontrollerPJ\Lab2\build_all.ps1
```

`set-target` only needs to run once per module; it creates `sdkconfig`. The
projects already contain a generated `sdkconfig` and a 4 MB flash configuration
in `sdkconfig.defaults` to match the Wokwi board.

## Run in Wokwi

1. In VS Code, open **the module folder itself** as the workspace, for example
   `C:\microcontrollerPJ\Lab2\module1_gpio_switch`. Do not open the whole
   `Lab2` folder, otherwise Wokwi looks for the wrong `diagram.json`.
2. Confirm `build/flasher_args.json` exists.
3. Run **Wokwi: Start Simulator**.
4. Open the serial monitor. Press the button (`b` key by default) or turn the
   potentiometer and watch the output.

`wokwi.toml` loads the full flash image, so the bootloader and partition table
are programmed as well as the application:

```toml
[wokwi]
version = 1
firmware = 'build/flasher_args.json'
elf = 'build/module1_gpio_switch.elf'
```

## Version control

The root `.gitignore` ignores the large ESP-IDF build tree but keeps the small
files Wokwi needs, so a fresh clone can run the simulator without rebuilding:

| Kept in git | Ignored |
|---|---|
| `main/main.c`, `main/CMakeLists.txt` | `build/CMakeCache.txt`, `build/CMakeFiles/`, `build/esp-idf/` |
| `CMakeLists.txt`, `sdkconfig`, `sdkconfig.defaults` | `build/*.map`, `build/*.ninja*`, `build/compile_commands.json` |
| `diagram.json`, `wokwi.toml` | `sdkconfig.old`, logs, editor folders |
| `build/flasher_args.json` | Other intermediate object files |
| `build/bootloader/bootloader.bin` | |
| `build/partition_table/partition-table.bin` | |
| `build/<project>.bin` and `build/<project>.elf` | |

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `firmware binary build/flasher_args.json not found` | `build/` missing or wrong folder open | Build the module, then open that module folder as the workspace |
| `idf.py` not recognized / scripts disabled | PowerShell execution policy | `Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass`, then activate ESP-IDF |
| Unicode warning from ESP-IDF | Windows code page | `$env:PYTHONUTF8 = "1"` before building |
| Wokwi shows a blank display | Wrong common type | This project uses a **common-cathode** display; keep `"common": "cathode"` |
| Counter jumps by more than one | Contact bounce | Keep the 50 ms ISR debounce; do not set `"bounce": "0"` in the diagram |
| Wokwi cannot find the board | Old Lab document board name | Use `board-esp32-devkit-c-v4` (with the hyphen before `c`) |

## Notes

- Build environment: ESP-IDF v6.1 at `C:\esp\v6.1\esp-idf`, `xtensa-esp-elf`
  GCC 15.2.0, target `esp32`.
- All three modules were configured with `idf.py set-target esp32` and built
  with `idf.py build`; the output folders in this repository are the result of
  those builds.
- No lab PDF/document was present in `C:\microcontrollerPJ\Lab2`, so the module
  breakdown follows the topics in the request: GPIO/interrupts, ADC/PWM, and
  multiplexed seven-segment.
