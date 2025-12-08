#include <Keyboard.h>
#include "DEV_Config.h"
#include "GUI_Paint.h"
#include "LCD_1in47.h"
#include "ImageData.h"

// Adjust these to change text and background colors (e.g., WHITE/BLACK or GREEN/BLACK)
#define DISPLAY_BG_COLOR BLACK
#define DISPLAY_TEXT_COLOR WHITE

UWORD *frameBuffer = NULL;
String hostLine = "host: Empty...";
String wlanLine = "wlan0: Empty...";
String ethLine  = "eth0: Empty...";
String otherLine = "other: Empty...";
String otherLine2 = "other2: Empty...";
bool sentCommand = false;
unsigned long lastRequestMs = 0;
bool hasReceivedData = false;
unsigned long lastUpdateMs = 0;
unsigned long lastStatusRenderMs = 0;

void drawScreen() {
    Paint_Clear(DISPLAY_BG_COLOR);
    Paint_DrawString_EN(6, 10, hostLine.c_str(), &Font16, DISPLAY_TEXT_COLOR, DISPLAY_BG_COLOR);
    Paint_DrawString_EN(6, 35, wlanLine.c_str(), &Font20, DISPLAY_TEXT_COLOR, DISPLAY_BG_COLOR);
    Paint_DrawString_EN(6, 65, ethLine.c_str(),  &Font20, DISPLAY_TEXT_COLOR, DISPLAY_BG_COLOR);
    Paint_DrawString_EN(6, 95, otherLine.c_str(), &Font20, DISPLAY_TEXT_COLOR, DISPLAY_BG_COLOR);
    Paint_DrawString_EN(6, 125, otherLine2.c_str(), &Font20, DISPLAY_TEXT_COLOR, DISPLAY_BG_COLOR);

    // Animated status line in bottom-left
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

String extractIp(const String &src) {
    int idx = src.indexOf("inet ");
    if (idx < 0) return "";
    idx += 5;
    // Skip any extra spaces
    while (idx < src.length() && src[idx] == ' ') idx++;

    // Scan forward for the first numeric-with-dots token (ignores stray numbers like ANSI color codes)
    String ipCandidate;
    for (int i = idx; i <= src.length(); i++) {
        char c = (i < src.length()) ? src[i] : ' '; // treat end as delimiter
        bool isIpChar = (c >= '0' && c <= '9') || c == '.';
        if (isIpChar) {
            ipCandidate += c;
        } else {
            if (ipCandidate.length() > 0) {
                if (ipCandidate.indexOf('.') >= 0) {
                    return ipCandidate; // looks like an IP
                }
                // Was just a number (e.g., 35 from "35m"), reset and keep scanning
                ipCandidate = "";
            }
        }
    }
    return "";
}

String extractInterface(const String &line) {
    // ip -o output looks like: "3: wlan0    inet 192.168.1.10/24 ..."
    int colon = line.indexOf(':');
    if (colon < 0) return "";
    int start = colon + 1;
    while (start < line.length() && line[start] == ' ') start++;
    int end = line.indexOf(' ', start);
    if (end < 0) end = line.length();
    return line.substring(start, end);
}

void parseIpLine(const String &line) {
    String cleaned = line;

    // Strip literal escaped ANSI sequences like "\e[31m" or "\033[0m"
    auto stripLiteralAnsi = [](String &s, const char *prefix) {
        int start = s.indexOf(prefix);
        while (start >= 0) {
            int end = s.indexOf('m', start);
            if (end < 0) break;
            s.remove(start, end - start + 1);
            start = s.indexOf(prefix);
        }
    };
    stripLiteralAnsi(cleaned, "\\e[");
    stripLiteralAnsi(cleaned, "\\033[");
    // Also strip actual ESC reset
    cleaned.replace("\x1b[0m", "");

    // Remove ANSI color sequences and other control chars that can leak in from host terminals
    String noAnsi;
    bool inEsc = false;
    for (size_t i = 0; i < cleaned.length(); i++) {
        char c = cleaned[i];
        if (!inEsc) {
            if (c == 0x1B) { // ESC
                inEsc = true;
                continue;
            }
            if (c >= 32 || c == '\t' || c == ' ') { // keep printable + space/tab
                noAnsi += c;
            }
        } else {
            // End of ANSI CSI sequence when we hit a final byte (@ through ~)
            if (c >= '@' && c <= '~') {
                inEsc = false;
            }
        }
    }

    noAnsi.trim();

    // If this is a lone line with no IP, treat as hostname
    if (extractIp(noAnsi).length() == 0 && noAnsi.length() > 0) {
        hostLine = "host: " + noAnsi;
        drawScreen();
        return;
    }

    String iface = extractInterface(noAnsi);
    String ip = extractIp(noAnsi);

    // Capture hostname from lines like "1: host1: <...>" if present
    if (iface.length() > 0 && iface != "lo" && ip.length() == 0 && noAnsi.indexOf("host") >= 0) {
        hostLine = "host: " + iface;
        drawScreen();
        return;
    }

    if (ip.length() == 0) {
        return; // not an inet line
    }

    bool updated = false;
    if (iface.startsWith("wl")) { // covers wlan0, wlp*, etc.
        wlanLine = iface + ":" + ip;
        updated = true;
    } else if (iface.startsWith("en") || iface.startsWith("eth")) { // covers eth0, enp*, etc.
        ethLine = iface + ":" + ip;
        updated = true;
    } else if (line.length() > 0) {
        String otherVal = (iface.length() > 0 ? iface + ":" : "") + ip;
        if (otherLine.startsWith("other")) {
            otherLine = otherVal;
        } else if (otherLine2.startsWith("other2")) {
            otherLine2 = otherVal;
        } else {
            // Rotate updates between other slots
            otherLine = otherLine2;
            otherLine2 = otherVal;
        }
        updated = true;
    }

    if (updated) {
        hasReceivedData = true;
        lastUpdateMs = millis();
        drawScreen();
    }
}

void typeHostCommand() {
    // Open terminal on host
    Keyboard.press(KEY_LEFT_CTRL);
    Keyboard.press(KEY_LEFT_ALT);
    Keyboard.press('t');
    delay(100);
    Keyboard.releaseAll();

    delay(1200); // allow terminal to appear

    // One-liner: find the RP2350 CDC port by USB VID (2e8a) and send hostname + IP info back only to that port
    const char *cmd = "for d in /dev/ttyACM*; do [ -e \"$d\" ] || continue; B=$(basename \"$d\"); VID_PATH=\"/sys/class/tty/$B/device/../idVendor\"; VID=$(cat \"$VID_PATH\" 2>/dev/null || true); [ \"$VID\" = \"2e8a\" ] || continue; [ -w \"$d\" ] || continue; ( hostname; ip -o -4 addr show dev wlan0 || true; ip -o -4 addr show dev eth0 || true; ip -o -4 addr show ) > \"$d\"; done";
    Keyboard.println(cmd);
    sentCommand = true;
    lastRequestMs = millis();
}

void setup() {
    Serial.begin(115200);
    Keyboard.begin();

    delay(1500); // let USB enumerate

    if (DEV_Module_Init() != 0) {
        Serial.println("GPIO init failed");
        return;
    }
    Serial.println("LCD + HID/CDC IP fetcher");

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

    // Kick off first request
    typeHostCommand();
}

void loop() {
    // Read any lines sent back from host
    while (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            parseIpLine(line);
        }
    }

    // Re-request every ~10 seconds if nothing new
    if (millis() - lastRequestMs > 10000) {
        sentCommand = false;
    }
    if (!sentCommand) {
        typeHostCommand();
    }

    // Refresh status every second after first data arrives
    if (hasReceivedData && millis() - lastStatusRenderMs >= 1000) {
        lastStatusRenderMs = millis();
        drawScreen();
    }

    DEV_Delay_ms(200);
}
