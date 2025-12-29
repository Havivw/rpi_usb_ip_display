# IP & MAC Address Display for RP2350 + Waveshare 1.47" LCD
> Requires the host system to be logged in.
> Now supports **Linux**, **macOS**, and **Windows**!

![usb_ip_connected](https://github.com/user-attachments/assets/b9c3098c-4b85-4226-8088-a0389cba1ab0)

Small Arduino sketch for RP2350 boards that:
- Opens a terminal on the host via USB HID keyboard emulation
- Runs OS-specific commands to gather hostname, IP addresses, and MAC addresses
- Sends output back over CDC (USB Serial) and displays on the 1.47" LCD

## Supported Operating Systems

| Version | Folder | Target OS | Terminal Shortcut |
|---------|--------|-----------|-------------------|
| `show_rpi_ip.ino` | `show_rpi_ip/` | **Linux** (Raspberry Pi OS, Ubuntu, Debian) | `Ctrl+Alt+T` |
| `show_rpi_ip_macos.ino` | `show_rpi_ip_macos/` | **macOS** | `Cmd+Space` → Terminal |
| `show_rpi_ip_windows.ino` | `show_rpi_ip_windows/` | **Windows** | `Win+R` → PowerShell |

## Hardware/Setup
- RP2350 board with the Waveshare 1.47" LCD module wired per the Waveshare examples.
- Place the Waveshare driver sources (e.g., `DEV_Config.*`, `GUI_Paint.*`, `LCD_1in47.*`, fonts, image data) alongside the `.ino`, or install them as an Arduino library.
- 3D printed shell shouldn't need support, and uses m2x4mm screws

## Display Layout
```
┌──────────────────────────────┐
│ host: raspberrypi            │  ← Hostname
│ wlan0: 192.168.1.100         │  ← WiFi IP
│   MAC: aa:bb:cc:dd:ee:ff     │  ← WiFi MAC
│ eth0: 192.168.1.101          │  ← Ethernet IP
│   MAC: 11:22:33:44:55:66     │  ← Ethernet MAC
│ Updated 5s                   │  ← Status
└──────────────────────────────┘
```

## Sketch Notes
- Display shows: hostname, WiFi IP+MAC, Ethernet IP+MAC, and update status
- Status line: shows `Connecting…` dots until first data, then `Updated <Xs>` with seconds since last update
- Host command filters CDC ports by USB VID `2e8a` (RP2350); adjust in `typeHostCommand()` if your VID differs
- Colors configurable at the top of each `.ino` via `DISPLAY_BG_COLOR` and `DISPLAY_TEXT_COLOR`

## Usage
1) Choose the correct folder for your target OS (Linux, macOS, or Windows)
2) Open the `.ino` file in Arduino IDE
3) Ensure Waveshare driver files are in the same folder (already included)
4) Select board: **Raspberry Pi Pico 2** (or your RP2350 variant)
5) Set **USB Stack** to **Adafruit TinyUSB** (required for HID keyboard)
6) Connect board in bootloader mode (hold BOOTSEL while plugging in)
7) Upload, or drag the `.uf2` file from `build/` folder onto the RPI-RP2 drive

## Pre-built Firmware
Ready-to-flash `.uf2` files are in each version's `build/` folder:
- `show_rpi_ip/build/show_rpi_ip.ino.uf2` - Linux
- `show_rpi_ip_macos/build/show_rpi_ip_macos.ino.uf2` - macOS  
- `show_rpi_ip_windows/build/show_rpi_ip_windows.ino.uf2` - Windows
