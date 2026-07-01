# pico-hid-ups

**[API Reference & Documentation](https://abyrne55.github.io/pico-hid-ups/)**

USB HID Power Device (UPS) library for RP2040 using the Pico SDK and TinyUSB.

Makes any RP2040 board appear as a USB UPS to operating systems like Linux (NUT),
macOS, Windows, and Synology DSM — complete with `GET_REPORT` support, real-time
status, and CDC serial for debug output.

## Features

- **HID always interface 0** — required by Synology NUT 0.41 (`wIndex=0`)
- **GET_REPORT returns real values** — `tud_hid_get_report_cb` handles all 27 report IDs
- **Type-safe API** — named setters instead of raw `sendReport()`
- **Host command callbacks** — register a function for `DelayBeforeShutdown` writes
- **CDC serial always available** — HID at interface 0, CDC at interfaces 1+2
- **Spec-correct flags** — RemainingTimeLimit and AudibleAlarmControl are FEATURE-only

## Prerequisites

- [Pico SDK](https://github.com/raspberrypi/pico-sdk) (1.5+)
- CMake 3.13+
- ARM GCC toolchain (`arm-none-eabi-gcc`)

## Quickstart

```cpp
#include "pico_hid_ups.h"
#include "tusb.h"

int main() {
    board_init();
    hid_ups.begin();
    tusb_init();

    while (true) {
        tud_task();

        hid_ups.status.ac_present = 1;
        hid_ups.status.battery_present = 1;
        hid_ups.set_remaining_capacity(85);
        hid_ups.set_runtime_to_empty(1800);

        hid_ups.send();
    }
}
```

## Integration

Add the library as a subdirectory in your `CMakeLists.txt`:

```cmake
add_subdirectory(../pico-hid-ups pico-hid-ups)
target_link_libraries(my_app pico_hid_ups)

# IMPORTANT: do NOT enable stdio_usb — the library owns USB descriptors
pico_enable_stdio_usb(my_app 0)
```

## Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Flash the resulting `.uf2` file to your RP2040 board.

## CDC serial

The CDC serial interface is always available at `/dev/ttyACM0`. Connect with:

```bash
picocom /dev/ttyACM0 --noreset
```

The `--noreset` flag is required — without it, picocom toggles DTR on open
which causes connection failures with TinyUSB CDC ACM devices.

## Why not pico_enable_stdio_usb?

The Pico SDK's `pico_enable_stdio_usb` provides its own `tusb_config.h` and USB
descriptor callbacks. Since this library must own the descriptor layout (HID at
interface 0), enabling `stdio_usb` would cause link-time conflicts. Use
`tud_cdc_write()` / `tud_cdc_write_flush()` directly for debug output — the CDC
interface is always available at `/dev/ttyACM0`.

## License

GPL-3.0 — see [LICENSE](LICENSE).
