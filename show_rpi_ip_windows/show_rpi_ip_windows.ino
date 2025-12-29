// Windows version - IP and MAC address display for RP2350 + Waveshare 1.47" LCD
// Requires the host system to be logged in to Windows
//
// LANGUAGE NOTE:
// The Run dialog (Win+R) and "powershell" command work in all Windows languages.
// The PowerShell commands (Get-NetAdapter, etc.) are also language-independent.
// No configuration needed for different languages!

#include <Keyboard.h>
#include "DEV_Config.h"
#include "GUI_Paint.h"
#include "LCD_1in47.h"
#include "ImageData.h"

// Adjust these to change text and background colors
#define DISPLAY_BG_COLOR BLACK
#define DISPLAY_TEXT_COLOR GREEN

UWORD *frameBuffer = NULL;
String hostLine = "host: Empty...";
String wlanLine = "Wi-Fi: Empty...";
String wlanMac  = "";
String ethLine  = "Ethernet: Empty...";
String ethMac   = "";
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

    // Status line
    String status;
    if (!hasReceivedData) {
        int dotCount = (millis() / 500) % 3;
        status = "Connecting.";
        for (int i = 0; i < dotCount; i++) status += ".";
    } else {
        unsigned long elapsedMs = millis() - lastUpdateMs;
        unsigned long secs = elapsedMs / 1000;
        status = "Updated " + String(secs) + "s";
    }
    Paint_DrawString_EN(2, LCD_1IN47_HEIGHT - 16, status.c_str(), &Font12, DISPLAY_TEXT_COLOR, DISPLAY_BG_COLOR);
    LCD_1IN47_Display(frameBuffer);
}

void parseResponse(const String &line) {
    String cleaned = line;
    cleaned.trim();
    
    if (cleaned.length() == 0) return;
    
    // Parse responses in format: TYPE:VALUE
    int colonIdx = cleaned.indexOf(':');
    if (colonIdx < 0) return;
    
    String type = cleaned.substring(0, colonIdx);
    String value = cleaned.substring(colonIdx + 1);
    value.trim();
    
    bool updated = false;
    
    if (type == "HOST") {
        hostLine = "host: " + value;
        updated = true;
    } else if (type == "WIFI_IP") {
        if (value.length() > 0 && value != "none") {
            wlanLine = "Wi-Fi:" + value;
            updated = true;
        }
    } else if (type == "WIFI_MAC") {
        if (value.length() > 0) {
            wlanMac = "MAC: " + value;
            updated = true;
        }
    } else if (type == "ETH_IP") {
        if (value.length() > 0 && value != "none") {
            ethLine = "Eth:" + value;
            updated = true;
        }
    } else if (type == "ETH_MAC") {
        if (value.length() > 0) {
            ethMac = "MAC: " + value;
            updated = true;
        }
    }
    
    if (updated) {
        hasReceivedData = true;
        lastUpdateMs = millis();
        drawScreen();
    }
}

void typeHostCommand() {
    // Windows: Open Run dialog with Win+R
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('r');
    delay(100);
    Keyboard.releaseAll();
    delay(500);
    
    // Type "powershell" and press Enter
    Keyboard.print("powershell");
    delay(200);
    Keyboard.press(KEY_RETURN);
    Keyboard.releaseAll();
    delay(1500); // Wait for PowerShell to open
    
    // PowerShell script to find RP2350 COM port and send network info
    // Note: Windows requires finding the COM port associated with the RP2350
    const char *cmd = "$ErrorActionPreference='SilentlyContinue'; $ports=Get-WmiObject Win32_PnPEntity|?{$_.Name -match 'COM\\d+' -and $_.DeviceID -match '2E8A'}; foreach($p in $ports){if($p.Name -match 'COM(\\d+)'){$cn=\"COM$($Matches[1])\"; try{$pt=New-Object System.IO.Ports.SerialPort $cn,115200; $pt.Open(); $pt.WriteLine(\"HOST:$env:COMPUTERNAME\"); $w=Get-NetAdapter|?{$_.Name -match 'Wi-Fi|Wireless' -and $_.Status -eq 'Up'}; if($w){$wip=(Get-NetIPAddress -InterfaceIndex $w.ifIndex -AddressFamily IPv4).IPAddress; $pt.WriteLine(\"WIFI_IP:$wip\"); $pt.WriteLine(\"WIFI_MAC:$($w.MacAddress)\")}; $e=Get-NetAdapter|?{$_.Name -match 'Ethernet' -and $_.Status -eq 'Up'}; if($e){$eip=(Get-NetIPAddress -InterfaceIndex $e.ifIndex -AddressFamily IPv4).IPAddress; $pt.WriteLine(\"ETH_IP:$eip\"); $pt.WriteLine(\"ETH_MAC:$($e.MacAddress)\")}; $pt.Close()}catch{}}}}; exit";
    
    Keyboard.println(cmd);
    sentCommand = true;
    lastRequestMs = millis();
}

void setup() {
    Serial.begin(115200);
    Keyboard.begin();
    delay(1500);

    if (DEV_Module_Init() != 0) {
        Serial.println("GPIO init failed");
        return;
    }
    Serial.println("LCD + HID/CDC IP fetcher (Windows)");

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

    typeHostCommand();
}

void loop() {
    while (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            parseResponse(line);
        }
    }

    if (millis() - lastRequestMs > 10000) {
        sentCommand = false;
    }
    if (!sentCommand) {
        typeHostCommand();
    }

    if (hasReceivedData && millis() - lastStatusRenderMs >= 1000) {
        lastStatusRenderMs = millis();
        drawScreen();
    }

    DEV_Delay_ms(200);
}

