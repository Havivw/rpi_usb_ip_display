// Universal Cross-Platform version - IP and MAC address display for RP2350 + Waveshare 1.47" LCD
// Works on Linux, macOS, and Windows from a single firmware!
//
// HOW IT WORKS:
// The device sends keyboard commands for ALL three operating systems in sequence.
// Only the correct OS will respond - the others will ignore or fail silently.
//
// Sequence:
// 1. Windows: Win+R → PowerShell command → exit
// 2. Linux: Ctrl+Alt+T → bash command → exit
// 3. macOS: Cmd+Space → Terminal → bash command → exit
//
// LANGUAGE CONFIG (for macOS Spotlight):
#define TERMINAL_APP_NAME "Terminal"  // Change for non-English macOS

#include <Keyboard.h>
#include "DEV_Config.h"
#include "GUI_Paint.h"
#include "LCD_1in47.h"
#include "ImageData.h"

// Display colors
#define DISPLAY_BG_COLOR BLACK
#define DISPLAY_TEXT_COLOR GREEN

UWORD *frameBuffer = NULL;
String hostLine = "Detecting OS...";
String wlanLine = "WiFi: --";
String wlanMac  = "";
String ethLine  = "Eth: --";
String ethMac   = "";
String detectedOS = "";
bool sentCommand = false;
unsigned long lastRequestMs = 0;
bool hasReceivedData = false;
unsigned long lastUpdateMs = 0;
unsigned long lastStatusRenderMs = 0;

void drawScreen() {
    Paint_Clear(DISPLAY_BG_COLOR);
    Paint_DrawString_EN(6, 8, hostLine.c_str(), &Font16, DISPLAY_TEXT_COLOR, DISPLAY_BG_COLOR);
    
    // WiFi IP and MAC
    Paint_DrawString_EN(6, 32, wlanLine.c_str(), &Font20, DISPLAY_TEXT_COLOR, DISPLAY_BG_COLOR);
    if (wlanMac.length() > 0) {
        Paint_DrawString_EN(10, 54, wlanMac.c_str(), &Font12, DISPLAY_TEXT_COLOR, DISPLAY_BG_COLOR);
    }
    
    // Ethernet IP and MAC
    Paint_DrawString_EN(6, 74, ethLine.c_str(), &Font20, DISPLAY_TEXT_COLOR, DISPLAY_BG_COLOR);
    if (ethMac.length() > 0) {
        Paint_DrawString_EN(10, 96, ethMac.c_str(), &Font12, DISPLAY_TEXT_COLOR, DISPLAY_BG_COLOR);
    }

    // Status line with OS indicator
    String status;
    if (!hasReceivedData) {
        int dotCount = (millis() / 500) % 4;
        status = "Scanning";
        for (int i = 0; i < dotCount; i++) status += ".";
    } else {
        unsigned long elapsedMs = millis() - lastUpdateMs;
        unsigned long secs = elapsedMs / 1000;
        status = detectedOS + " " + String(secs) + "s";
    }
    Paint_DrawString_EN(2, LCD_1IN47_HEIGHT - 16, status.c_str(), &Font12, DISPLAY_TEXT_COLOR, DISPLAY_BG_COLOR);
    LCD_1IN47_Display(frameBuffer);
}

// Parse unified response format: TYPE:VALUE
void parseResponse(const String &line) {
    String cleaned = line;
    cleaned.trim();
    
    // Remove ANSI escape sequences
    String noAnsi;
    bool inEsc = false;
    for (size_t i = 0; i < cleaned.length(); i++) {
        char c = cleaned[i];
        if (!inEsc) {
            if (c == 0x1B) { inEsc = true; continue; }
            if (c >= 32) noAnsi += c;
        } else {
            if (c >= '@' && c <= '~') inEsc = false;
        }
    }
    noAnsi.trim();
    
    if (noAnsi.length() == 0) return;
    
    // Look for TYPE:VALUE format
    int colonIdx = noAnsi.indexOf(':');
    if (colonIdx > 0 && colonIdx < 15) {
        String type = noAnsi.substring(0, colonIdx);
        String value = noAnsi.substring(colonIdx + 1);
        value.trim();
        
        bool updated = false;
        
        if (type == "OS") {
            detectedOS = value;
            updated = true;
        } else if (type == "HOST") {
            hostLine = "host: " + value;
            updated = true;
        } else if (type == "WIFI_IP") {
            if (value.length() > 0 && value != "none" && value != "") {
                wlanLine = "WiFi:" + value;
                updated = true;
            }
        } else if (type == "WIFI_MAC") {
            if (value.length() > 5) {
                wlanMac = "MAC:" + value;
                updated = true;
            }
        } else if (type == "ETH_IP") {
            if (value.length() > 0 && value != "none" && value != "") {
                ethLine = "Eth:" + value;
                updated = true;
            }
        } else if (type == "ETH_MAC") {
            if (value.length() > 5) {
                ethMac = "MAC:" + value;
                updated = true;
            }
        }
        
        if (updated) {
            hasReceivedData = true;
            lastUpdateMs = millis();
            drawScreen();
        }
        return;
    }
    
    // Fallback: treat as hostname if no colon found
    if (noAnsi.length() > 0 && noAnsi.length() < 50 && noAnsi.indexOf(' ') < 0) {
        hostLine = "host: " + noAnsi;
        hasReceivedData = true;
        lastUpdateMs = millis();
        drawScreen();
    }
}

void typeWindowsCommand() {
    // Windows: Win+R to open Run dialog
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('r');
    delay(100);
    Keyboard.releaseAll();
    delay(600);
    
    // PowerShell one-liner that outputs in TYPE:VALUE format
    // Uses -WindowStyle Hidden to minimize visibility
    const char *cmd = "powershell -NoProfile -Command \"$ErrorActionPreference='SilentlyContinue'; $p=Get-WmiObject Win32_PnPEntity|?{$_.Name -match 'COM\\d+' -and $_.DeviceID -match '2E8A'}; foreach($x in $p){if($x.Name -match 'COM(\\d+)'){$c='COM'+$Matches[1]; try{$s=New-Object System.IO.Ports.SerialPort $c,115200; $s.Open(); $s.WriteLine('OS:Win'); $s.WriteLine('HOST:'+$env:COMPUTERNAME); $w=Get-NetAdapter|?{$_.Name -match 'Wi-Fi|Wireless' -and $_.Status -eq 'Up'}; if($w){$s.WriteLine('WIFI_IP:'+(Get-NetIPAddress -InterfaceIndex $w.ifIndex -AddressFamily IPv4 -EA 0).IPAddress); $s.WriteLine('WIFI_MAC:'+$w.MacAddress)}; $e=Get-NetAdapter|?{$_.Name -match 'Ethernet' -and $_.Status -eq 'Up'}; if($e){$s.WriteLine('ETH_IP:'+(Get-NetIPAddress -InterfaceIndex $e.ifIndex -AddressFamily IPv4 -EA 0).IPAddress); $s.WriteLine('ETH_MAC:'+$e.MacAddress)}; $s.Close()}catch{}}}}\"";
    Keyboard.println(cmd);
}

void typeLinuxCommand() {
    // Linux: Ctrl+Alt+T to open terminal
    Keyboard.press(KEY_LEFT_CTRL);
    Keyboard.press(KEY_LEFT_ALT);
    Keyboard.press('t');
    delay(100);
    Keyboard.releaseAll();
    delay(1200);
    
    // Bash one-liner that outputs in TYPE:VALUE format
    const char *cmd = "for d in /dev/ttyACM*; do [ -e \"$d\" ] || continue; B=$(basename \"$d\"); V=$(cat /sys/class/tty/$B/device/../idVendor 2>/dev/null); [ \"$V\" = \"2e8a\" ] || continue; [ -w \"$d\" ] || continue; ( echo \"OS:Linux\"; echo \"HOST:$(hostname)\"; WIP=$(ip -4 addr show wlan0 2>/dev/null | grep -oP 'inet \\K[\\d.]+'); [ -n \"$WIP\" ] && echo \"WIFI_IP:$WIP\"; WMAC=$(cat /sys/class/net/wlan0/address 2>/dev/null); [ -n \"$WMAC\" ] && echo \"WIFI_MAC:$WMAC\"; EIP=$(ip -4 addr show eth0 2>/dev/null | grep -oP 'inet \\K[\\d.]+'); [ -n \"$EIP\" ] && echo \"ETH_IP:$EIP\"; EMAC=$(cat /sys/class/net/eth0/address 2>/dev/null); [ -n \"$EMAC\" ] && echo \"ETH_MAC:$EMAC\" ) > \"$d\"; break; done; exit";
    Keyboard.println(cmd);
}

void typeMacOSCommand() {
    // macOS: Cmd+Space to open Spotlight
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press(' ');
    delay(100);
    Keyboard.releaseAll();
    delay(600);
    
    // Type Terminal app name and open it
    Keyboard.print(TERMINAL_APP_NAME);
    delay(400);
    Keyboard.press(KEY_RETURN);
    Keyboard.releaseAll();
    delay(1500);
    
    // Bash one-liner for macOS
    const char *cmd = "for d in /dev/cu.usbmodem*; do [ -e \"$d\" ] || continue; ( echo \"OS:macOS\"; echo \"HOST:$(hostname)\"; WIP=$(ipconfig getifaddr en0 2>/dev/null); [ -n \"$WIP\" ] && echo \"WIFI_IP:$WIP\"; WMAC=$(ifconfig en0 2>/dev/null | awk '/ether/{print $2}'); [ -n \"$WMAC\" ] && echo \"WIFI_MAC:$WMAC\"; EIP=$(ipconfig getifaddr en1 2>/dev/null); [ -z \"$EIP\" ] && EIP=$(ipconfig getifaddr en2 2>/dev/null); [ -n \"$EIP\" ] && echo \"ETH_IP:$EIP\"; EMAC=$(ifconfig en1 2>/dev/null | awk '/ether/{print $2}'); [ -z \"$EMAC\" ] && EMAC=$(ifconfig en2 2>/dev/null | awk '/ether/{print $2}'); [ -n \"$EMAC\" ] && echo \"ETH_MAC:$EMAC\" ) > \"$d\" 2>/dev/null; break; done; exit";
    Keyboard.println(cmd);
}

void typeAllPlatformCommands() {
    // Try all three platforms in sequence
    // Each will fail silently on the wrong OS
    
    // 1. Windows (Win+R is ignored on Linux/macOS)
    typeWindowsCommand();
    delay(2000);
    
    // 2. Linux (Ctrl+Alt+T is usually ignored on Windows/macOS)
    typeLinuxCommand();
    delay(2000);
    
    // 3. macOS (Cmd+Space might open Spotlight even on failure, but Terminal won't exist on Windows)
    typeMacOSCommand();
    
    sentCommand = true;
    lastRequestMs = millis();
}

void setup() {
    Serial.begin(115200);
    Keyboard.begin();
    delay(2000); // Extra time for USB enumeration

    if (DEV_Module_Init() != 0) {
        Serial.println("GPIO init failed");
        return;
    }
    Serial.println("Universal IP Display - All Platforms");

    DEV_SET_PWM(0);
    LCD_1IN47_Init(VERTICAL);
    LCD_1IN47_Clear(WHITE);
    DEV_SET_PWM(80);

    UDOUBLE imageSize = LCD_1IN47_HEIGHT * LCD_1IN47_WIDTH * 2;
    frameBuffer = (UWORD *)malloc(imageSize);
    if (!frameBuffer) {
        Serial.println("Frame buffer alloc failed");
        return;
    }

    Paint_NewImage((UBYTE *)frameBuffer, LCD_1IN47.WIDTH, LCD_1IN47.HEIGHT, 0, WHITE);
    Paint_SetScale(65);
    Paint_SetRotate(ROTATE_0);
    Paint_Clear(WHITE);
    drawScreen();

    // Initial scan for all platforms
    typeAllPlatformCommands();
}

void loop() {
    // Read responses
    while (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            parseResponse(line);
        }
    }

    // Re-scan every 15 seconds (longer interval since we try 3 platforms)
    if (millis() - lastRequestMs > 15000) {
        sentCommand = false;
    }
    if (!sentCommand) {
        typeAllPlatformCommands();
    }

    // Refresh status every second
    if (hasReceivedData && millis() - lastStatusRenderMs >= 1000) {
        lastStatusRenderMs = millis();
        drawScreen();
    }

    DEV_Delay_ms(200);
}

