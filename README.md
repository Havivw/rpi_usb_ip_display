# IP address display for RP2350 + Waveshare 1.47" LCD
> Only tested on a RPi running Raspberry pi OS trixie.

Small Arduino sketch for RP2350 boards that:
- Opens a terminal on the host via USB HID (Ctrl+Alt+T), runs `hostname` and `ip -o -4 addr show`, and sends the output back over CDC.
- Parses the hostname and IPv4 addresses, displays them on the 1.47" LCD, and shows a status line (connecting, then seconds since last update).


## Hardware/Setup
- RP2350 board with the Waveshare 1.47" LCD module wired per the Waveshare examples.
- Place the Waveshare driver sources (e.g., `DEV_Config.*`, `GUI_Paint.*`, `LCD_1in47.*`, fonts, image data) alongside this `.ino`, or install them as an Arduino library.
- 3D printed shell shouldn't need support, and uses m2x4mm screws

## Sketch Notes
- Four display lines: hostname, Wi‑Fi (`wl*`), Ethernet (`en*/eth*`), and two “other” slots for additional detected interfaces.
- Status line: shows `Connecting…` dots until first data, then `Updated <Xs>` with seconds since last update (refreshes every second).
- Host command filters CDC ports by USB VID `2e8a` (RP2350); adjust in `typeHostCommand()` if your VID differs.
- Colors are configurable at the top of `CompositeIpFetcher.ino` via `DISPLAY_BG_COLOR` and `DISPLAY_TEXT_COLOR`.

## Usage
1) Open `CompositeIpFetcher.ino` in the Arduino IDE or your toolchain.
2) Ensure the Waveshare driver files are available to the build (same folder as the sketch or installed as a library).
3) Connect the board, select the correct port/board, and upload.
    (To make sure it doesn't run on system hold bootloader button when connecting the board)
4) On boot, the sketch opens a terminal on the host, gathers hostname/IPs, and updates the screen every ~10 seconds or when new data arrives.
