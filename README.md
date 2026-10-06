# ESP32-2424S012C-Y(B)

Input-language indicator and switcher for the black-shell ESP32-C3 round
display identified by the package label `ESP32-2424S012C-Y(B)` / SKU
`10103002`.

The display shows the current input language. A single tap selects the next
keyboard layout. The current setup maps Russian to a Russian flag and English
to a US flag.

At power-on the display shows the white pear logo for about 0.9 seconds before
the first flag. The source SVG is `assets/pear_logo_master.svg`; its generated
one-bit firmware mask is `src/pear_logo_bitmap.h`.

The firmware supports two ways to switch and synchronize the layout:

- USB on the configured Mac: the LaunchAgent changes the real macOS input
  source and sends the exact current language back to the display.
- Bluetooth on a new Mac, Windows PC, or Linux PC: the device works as a BLE
  HID controller. Its default `AUTO` mode uses the standard Consumer HID
  command `AC Next Keyboard Layout Select` on hosts that subscribe to it.
  Current macOS versions parse that command but do not act on it, so the device
  detects the macOS subscription pattern and falls back to Command+Space.
  The optional Round Display Helper writes the exact current language to a
  dedicated BLE characteristic on connect and after every input-source change.

The Bluetooth device name is `Round Language Switch`. The gestures are:

- Short tap anywhere: switch the input language.
- Hold for about 0.7–1.5 seconds: change only the displayed RU/EN flag without
  sending a key. Use this once to align the flag with the computer.
- Hold the center for about 1.6 seconds: delete old Bluetooth bonds and enter
  `PAIR / READY` mode for a new computer.
- Keep holding the center for about 5 seconds: show an installation QR code.
  Scan it to open the macOS helper download page. A tap closes the QR screen.
- Hold the outer ring for about 1.6 seconds: cycle
  `AUTO → MAC → WIN → LINUX`.

`AUTO` is the normal mode and is selected automatically by this firmware
update. `MAC`, `WIN`, and `LINUX` remain as compatibility fallbacks. All three
manual modes currently send GUI/Command/Super+Space. The chosen mode is kept
after power-off.

For reliable HID operation, the firmware waits for both BLE encryption and the
computer's notification subscription before accepting a switch command. It
requests a 7.5–15 ms connection interval with zero peripheral latency and sends
redundant press/release reports roughly 100 ms apart. A tap received while the
HID connection is still becoming ready is queued for up to 2 seconds instead
of being silently lost.

When `PAIR / READY` appears, the firmware has already deleted every old bond,
disconnected the current host, and started advertising again. The confirmation
remains visible for about 5 seconds, even when the USB bridge sends a language
update. Pair `Round Language Switch` on the new computer. If that computer
already contains an entry from an earlier attempt, forget that entry first so
it does not retain an obsolete encryption key.

Without the helper, standalone Bluetooth mode toggles locally between RU and EN
and saves the result in flash. If the layout is changed by another keyboard or
from the OS menu, hold the display for 0.7–1.5 seconds to align it again. With
the helper installed, Bluetooth and USB both restore exact synchronization
automatically on connect and after every macOS input-source change.

The macOS bridge runs automatically as the LaunchAgent
`com.rounddisplay.input-language-helper`. It supports Apple Silicon and Intel,
and updates the display through BLE and USB.

## macOS helper and GitHub releases

Build the universal installer archive locally with:

```sh
tools/build_macos_release.sh
```

The result is `dist/RoundDisplayHelper-macOS.zip`. After extracting it, open
`Install Round Display Helper.command`. It installs the background app into
`~/Applications`, registers its LaunchAgent, removes the previous USB-only
LaunchAgent, and starts the helper. macOS asks once for Bluetooth access.

The GitHub Actions workflow `.github/workflows/release.yml` builds and attaches
the same archive whenever a `v*` tag is pushed. The installation page is
published from `docs` at
`https://pearfresh.github.io/round-display-language-switch/`; its download
button always points to the latest release.

ESP32-C3 cannot expose that page as a USB flash drive: its native USB block is
fixed to Serial/JTAG and cannot implement USB Mass Storage. The firmware embeds
the installation URL as a QR code for first-time setup instead.

## Bluetooth setup

1. Power the display from USB or a battery.
2. Open Bluetooth settings on the computer.
3. Pair `Round Language Switch` once.
4. Leave the device in `AUTO`. Only if a host does not implement the standard
   command, hold the outer ring for 1.6 seconds to select a fallback mode.

On another computer, only pairing is needed; the firmware and flags remain on
the device.

## Hardware

- MCU: ESP32-C3, 4 MB flash
- RAM: 320 KB
- Display: GC9A01, 240 x 240, SPI
- Touch: CST816D/S, I2C address `0x15`
- USB serial port on this Mac: `/dev/cu.usbmodem11201`

The 4 MB flash is partitioned into 20 KB of settings/Bluetooth data, two
1.25 MB application slots (for safe OTA updates), 1.375 MB of SPIFFS file
storage, and small OTA/coredump service partitions. The current application
image is about 752 KB. A raw 240 x 240 RGB565 flag takes 115,200 bytes, so a
large flag library must use compact procedural, palette/RLE, or compressed
assets rather than uncompressed bitmaps.

| Function | GPIO |
| --- | ---: |
| LCD SCLK | 6 |
| LCD MOSI | 7 |
| LCD DC | 2 |
| LCD CS | 10 |
| LCD backlight | 3 |
| Touch SDA | 4 |
| Touch SCL | 5 |
| Touch interrupt | 0 |
| Touch reset | 1 |

## Commands

```sh
.pioenv/bin/python -m platformio run
.pioenv/bin/python -m platformio run --target upload
.pioenv/bin/python -m platformio device monitor
```

To regenerate the embedded logo and its 240 x 240 preview, run:

```sh
/Users/pear/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3 \
  tools/generate_logo_bitmap.py assets/pear_logo_master.svg \
  src/pear_logo_bitmap.h assets/pear_logo_boot_preview.png
```

The helper source is `host/InputLanguageBridge.m`. Its installed app is
`~/Applications/Round Display Helper.app`, and its log is
`~/Library/Logs/RoundDisplay/helper.log`.

For maintenance over the 115200-baud USB serial port, `BLE RECONNECT` restarts
advertising without deleting bonds, while `BLE PAIR` clears all old bonds and
opens a five-second `PAIR / READY` window for a new host.

The verified 4 MB factory image is stored at
`backup/esp32c3-factory-70af0918d688.bin`.
