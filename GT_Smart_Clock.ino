/*
 * Project: GT_Smart_Clock
 * Description: Smart Digital Clock with Max7219
 *
 * Author  : Emre Ogut
 * Date    : 25/04/2026
 * Version : 1.0.0
 *
 * Target Hardware: ESP32
 *
 * License: MIT License (or specify your preferred license)
 *
 * Change Log:
 * v1.0.0 - Initial release
 */
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WebServer.h>

// ================= WIFI =================
const char* ssid = "***********";
const char* password =  "***********";

// ================= NTP =================
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 60000);

// ================= MAX7219 =================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN 18
#define DATA_PIN 23
#define CS_PIN 5

MD_Parola display = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// ================= WEB =================
WebServer server(80);

// ================= SETTINGS =================
int brightness = 3;

// ================= TEXT MODE =================
String incomingText = "";
int scrollSpeed = 50;
int repeatCount = 1;

bool showTextMode = false;
int currentRepeat = 0;
uint32_t prevTimeAnim = 0;

// ================= CLOCK =================
bool colonState = true;
unsigned long lastBlink = 0;

// ================= ANIMATION =================
enum State {
  BOOT,
  CLOCK,
  TEXT_MODE
};

State state = BOOT;

// ================= ANIMATION SELECT =================
uint8_t selectedAnimation = 0; // 0 = idle
bool animationActive = false;
bool animationRestart = true;
unsigned long animationStartMs = 0;

// Animation timing (ms) for one-shot execution
const uint16_t animationDurationMs[] = {
  0,    // idle
  10000,  // graphicMidline1
  10000, // graphicMidline2
  10000, // graphicScanner
  10000, // graphicRandom
  10000, // graphicFade
  10000, // graphicSpectrum1
  10000, // graphicHeartbeat
  10000, // graphicHearts
  10000, // graphicEyes
  10000, // graphicBounceBall
  10000, // graphicArrowScroll
  10000, // graphicScroller
  10000, // graphicWiper
  10000, // graphicInvader
  10000, // graphicPacman
  10000, // graphicArrowRotate
  10000, // graphicSpectrum2
  10000, // graphicSinewave
  2000, // graphicRightWink
  2000  // graphicLeftWink
};

#define MX (*display.getGraphicObject())

// Forward declarations
void resetMatrix();
bool graphicMidline1(bool bInit);
bool graphicMidline2(bool bInit);
bool graphicScanner(bool bInit);
bool graphicRandom(bool bInit);
bool graphicFade(bool bInit);
bool graphicSpectrum1(bool bInit);
bool graphicHeartbeat(bool bInit);
bool graphicHearts(bool bInit);
bool graphicEyes(bool bInit);
bool graphicBounceBall(bool bInit);
bool graphicArrowScroll(bool bInit);
bool graphicScroller(bool bInit);
bool graphicWiper(bool bInit);
bool graphicInvader(bool bInit);
bool graphicPacman(bool bInit);
bool graphicArrowRotate(bool bInit);
bool graphicSpectrum2(bool bInit);
bool graphicSinewave(bool bInit);
bool graphicRightWink(bool bInit);
bool graphicLeftWink(bool bInit);
String sanitizeForHtml(String text);
String normalizeTurkishChars(String text);

// ================= HTML =================
String htmlPage() {
  String html = "<!doctype html><html><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>GT Smart Clock</title>";
  html += "<style>";
  html += "*{box-sizing:border-box}";
  html += "body{margin:0;font-family:Arial,sans-serif;background:linear-gradient(135deg,#0f172a,#1e293b);color:#e2e8f0;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px}";
  html += ".card{width:100%;max-width:460px;background:#0b1220cc;border:1px solid #334155;border-radius:16px;padding:20px;box-shadow:0 10px 30px rgba(0,0,0,.35)}";
  html += "h2{margin:0 0 6px;font-size:22px}";
  html += ".sub{margin:0 0 18px;color:#94a3b8;font-size:13px}";
  html += ".group{margin-bottom:14px}";
  html += "label{display:block;margin-bottom:6px;font-size:13px;color:#cbd5e1}";
  html += "input,select{width:100%;padding:11px 12px;border:1px solid #334155;border-radius:10px;background:#0f172a;color:#f8fafc;outline:none}";
  html += "input:focus,select:focus{border-color:#38bdf8;box-shadow:0 0 0 2px #0ea5e933}";
  html += ".row{display:grid;grid-template-columns:1fr 1fr;gap:10px}";
  html += "input[type=range]{padding:0;height:34px}";
  html += ".rangeInfo{margin-top:4px;font-size:12px;color:#94a3b8}";
  html += ".btn{width:100%;padding:12px;border:0;border-radius:10px;background:linear-gradient(90deg,#06b6d4,#3b82f6);color:#fff;font-weight:700;cursor:pointer}";
  html += ".btn:hover{filter:brightness(1.08)}";
  html += ".footer{margin-top:14px;font-size:12px;color:#94a3b8;text-align:center}";
  html += "</style></head><body>";

  html += "<div class='card'>";
  html += "<h2>GT Smart Clock</h2>";
  html += "<p class='sub'>Metin, parlaklik ve animasyon kontrol paneli</p>";

  html += "<form action='/send'>";

  html += "<div class='group'><label for='t'>Text</label>";
  html += "<input id='t' name='t' placeholder='Yazi girin' value='" + sanitizeForHtml(incomingText) + "'></div>";

  html += "<div class='row'>";
  html += "<div class='group'><label for='s'>Speed</label><input id='s' name='s' type='number' min='1' value='" + String(scrollSpeed) + "'></div>";
  html += "<div class='group'><label for='r'>Repeat</label><input id='r' name='r' type='number' min='1' value='" + String(repeatCount) + "'></div>";
  html += "</div>";

  html += "<div class='group'><label for='b'>Brightness</label>";
  html += "<input id='b' type='range' name='b' min='0' max='15' value='" + String(brightness) + "'>";
  html += "<div class='rangeInfo'>Seviye: <span id='bVal'>" + String(brightness) + "</span>/15</div></div>";

  html += "<div class='group'><label for='anim'>Animation</label><select id='anim' name='anim'>";
  html += "<option value='0'" + String(selectedAnimation == 0 ? " selected" : "") + ">idle</option>";
  html += "<option value='1'" + String(selectedAnimation == 1 ? " selected" : "") + ">Midline 1</option>";
  html += "<option value='2'" + String(selectedAnimation == 2 ? " selected" : "") + ">Midline 2</option>";
  html += "<option value='3'" + String(selectedAnimation == 3 ? " selected" : "") + ">Scanner</option>";
  html += "<option value='4'" + String(selectedAnimation == 4 ? " selected" : "") + ">Random</option>";
  html += "<option value='5'" + String(selectedAnimation == 5 ? " selected" : "") + ">Fade</option>";
  html += "<option value='6'" + String(selectedAnimation == 6 ? " selected" : "") + ">Spectrum 1</option>";
  html += "<option value='7'" + String(selectedAnimation == 7 ? " selected" : "") + ">Heartbeat</option>";
  html += "<option value='8'" + String(selectedAnimation == 8 ? " selected" : "") + ">Hearts</option>";
  html += "<option value='9'" + String(selectedAnimation == 9 ? " selected" : "") + ">Eyes</option>";
  html += "<option value='10'" + String(selectedAnimation == 10 ? " selected" : "") + ">Bounce Ball</option>";
  html += "<option value='11'" + String(selectedAnimation == 11 ? " selected" : "") + ">Arrow Scroll</option>";
  html += "<option value='12'" + String(selectedAnimation == 12 ? " selected" : "") + ">Scroller</option>";
  html += "<option value='13'" + String(selectedAnimation == 13 ? " selected" : "") + ">Wiper</option>";
  html += "<option value='14'" + String(selectedAnimation == 14 ? " selected" : "") + ">Invader</option>";
  html += "<option value='15'" + String(selectedAnimation == 15 ? " selected" : "") + ">Pacman</option>";
  html += "<option value='16'" + String(selectedAnimation == 16 ? " selected" : "") + ">Arrow Rotate</option>";
  html += "<option value='17'" + String(selectedAnimation == 17 ? " selected" : "") + ">Spectrum 2</option>";
  html += "<option value='18'" + String(selectedAnimation == 18 ? " selected" : "") + ">Sinewave</option>";
  html += "<option value='19'" + String(selectedAnimation == 19 ? " selected" : "") + ">Right Wink</option>";
  html += "<option value='20'" + String(selectedAnimation == 20 ? " selected" : "") + ">Left Wink</option>";
  html += "</select></div>";

  html += "<div class='row'>";
  html += "<button class='btn' type='submit' name='apply' value='text'>Text Uygula</button>";
  html += "<button class='btn' type='submit' name='apply' value='animation'>Animation Oynat</button>";
  html += "</div>";
  html += "<div class='row' style='margin-top:10px'>";
  html += "<button class='btn' type='submit' name='action' value='idle'>Saat (Idle)</button>";
  html += "<button class='btn' type='submit' name='action' value='clear'>Ekrani Temizle</button>";
  html += "</div>";
  html += "</form>";

  html += "<div class='footer'>IP: " + WiFi.localIP().toString() + "</div>";
  html += "</div>";

  html += "<script>";
  html += "const b=document.getElementById('b');";
  html += "const bVal=document.getElementById('bVal');";
  html += "b.addEventListener('input',()=>{bVal.textContent=b.value;});";
  html += "</script>";
  html += "</body></html>";

  return html;
}

// ================= WEB =================
void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleSend() {
  bool hasAction = server.hasArg("action");
  String action = hasAction ? server.arg("action") : "";
  String apply = server.hasArg("apply") ? server.arg("apply") : "";

  if (server.hasArg("t")) incomingText = normalizeTurkishChars(server.arg("t"));
  if (server.hasArg("s")) {
    long s = server.arg("s").toInt();
    scrollSpeed = (s < 1) ? 1 : (int)s;
  }
  if (server.hasArg("r")) {
    long r = server.arg("r").toInt();
    repeatCount = (r < 1) ? 1 : (int)r;
  }

  if (server.hasArg("b")) {
    brightness = server.arg("b").toInt();
    display.setIntensity(brightness);
  }

  if (action == "idle") {
    animationActive = false;
    selectedAnimation = 0;
    showTextMode = false;
    state = CLOCK;
  } else if (action == "clear") {
    animationActive = false;
    selectedAnimation = 0;
    showTextMode = false;
    display.displayClear();
    state = CLOCK;
  } else if (apply == "animation") {
    int anim = server.hasArg("anim") ? server.arg("anim").toInt() : 0;
    if (anim < 0) anim = 0;
    if (anim > 20) anim = 20;

    selectedAnimation = (uint8_t)anim;
    if (selectedAnimation > 0) {
      showTextMode = false;
      animationActive = true;
      animationRestart = true;
      animationStartMs = millis();
      state = CLOCK;
    } else {
      animationActive = false;
      selectedAnimation = 0;
      showTextMode = false;
      state = CLOCK;
    }
  } else if (apply == "text") {
    animationActive = false;
    selectedAnimation = 0;
    currentRepeat = 0;
    showTextMode = true;
    state = TEXT_MODE;
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

// ================= TIME =================
String getTime() {
  int h = timeClient.getHours();
  int m = timeClient.getMinutes();

  if (millis() - lastBlink > 500) {
    colonState = !colonState;
    lastBlink = millis();
  }

  String t;

  if (h < 10) t += "0";
  t += String(h);
  t += colonState ? ":" : " ";
  if (m < 10) t += "0";
  t += String(m);

  return t;
}

// ================= SCROLL =================
void scrollText(String txt, int speed) {
  display.displayText(txt.c_str(), PA_LEFT, speed, 1000, PA_SCROLL_LEFT, PA_SCROLL_LEFT);

  while (!display.displayAnimate()) {
    delay(5);
  }

  display.displayClear();
}

String sanitizeForHtml(String text) {
  text.replace("&", "&amp;");
  text.replace("\"", "&quot;");
  text.replace("<", "&lt;");
  text.replace(">", "&gt;");
  text.replace("'", "&#39;");
  return text;
}

String normalizeTurkishChars(String text) {
  text.replace("\xC3\xA7", "c"); // c
  text.replace("\xC3\x87", "C"); // C
  text.replace("\xC4\x9F", "g"); // g
  text.replace("\xC4\x9E", "G"); // G
  text.replace("\xC4\xB1", "i"); // dotless i
  text.replace("\xC4\xB0", "I"); // I with dot
  text.replace("\xC3\xB6", "o"); // o
  text.replace("\xC3\x96", "O"); // O
  text.replace("\xC5\x9F", "s"); // s
  text.replace("\xC5\x9E", "S"); // S
  text.replace("\xC3\xBC", "u"); // u
  text.replace("\xC3\x9C", "U"); // U
  return text;
}

// ================= BOOT ANIMATION =================
void bootAnimation() {

  // 1. Connecting WiFi effect
  scrollText("  OGUT ", 50);

  // 2. Blink effect (simple flash)
  for (int i = 0; i < 3; i++) {
    display.displayClear();
    delay(150);
    display.displayText("OGUTS", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    display.displayAnimate();
    delay(200);
  }

  // 3. Eyes animation for 5 seconds
  bool eyeRestart = true;
  unsigned long eyeStart = millis();
  while (millis() - eyeStart < 5000) {
    eyeRestart = graphicEyes(eyeRestart);
    delay(5);
  }
  display.displayClear();

  // 4. Show IP
  String ip = "IP:" + WiFi.localIP().toString();
  scrollText(ip, 40);
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi OK");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.update();

  display.begin();
  display.setIntensity(brightness);
  display.displayClear();
  randomSeed(micros());

  // BOOT ANIMATION
  bootAnimation();

  server.on("/", handleRoot);
  server.on("/send", handleSend);
  server.begin();

  state = CLOCK;
}

// ================= LOOP =================
void loop() {
  server.handleClient();

  // TEXT MODE
  if (showTextMode) {
    scrollText(incomingText, scrollSpeed);
    currentRepeat++;

    if (currentRepeat >= repeatCount) {
      showTextMode = false;
      state = CLOCK;
    }

    return;
  }

  if (animationActive && selectedAnimation > 0) {
    switch (selectedAnimation) {
      case 1:  animationRestart = graphicMidline1(animationRestart);    break;
      case 2:  animationRestart = graphicMidline2(animationRestart);    break;
      case 3:  animationRestart = graphicScanner(animationRestart);     break;
      case 4:  animationRestart = graphicRandom(animationRestart);      break;
      case 5:  animationRestart = graphicFade(animationRestart);        break;
      case 6:  animationRestart = graphicSpectrum1(animationRestart);   break;
      case 7:  animationRestart = graphicHeartbeat(animationRestart);   break;
      case 8:  animationRestart = graphicHearts(animationRestart);      break;
      case 9:  animationRestart = graphicEyes(animationRestart);        break;
      case 10: animationRestart = graphicBounceBall(animationRestart);  break;
      case 11: animationRestart = graphicArrowScroll(animationRestart); break;
      case 12: animationRestart = graphicScroller(animationRestart);    break;
      case 13: animationRestart = graphicWiper(animationRestart);       break;
      case 14: animationRestart = graphicInvader(animationRestart);     break;
      case 15: animationRestart = graphicPacman(animationRestart);      break;
      case 16: animationRestart = graphicArrowRotate(animationRestart); break;
      case 17: animationRestart = graphicSpectrum2(animationRestart);   break;
      case 18: animationRestart = graphicSinewave(animationRestart);    break;
      case 19: animationRestart = graphicRightWink(animationRestart);   break;
      case 20: animationRestart = graphicLeftWink(animationRestart);    break;
      default: animationActive = false; selectedAnimation = 0; break;
    }

    if (millis() - animationStartMs >= animationDurationMs[selectedAnimation]) {
      animationActive = false;
      selectedAnimation = 0;
      animationRestart = true;
      display.displayClear();
    }
    return;
  }

  // CLOCK MODE
  static unsigned long lastNtp = 0;
  if (millis() - lastNtp > 60000) {
    lastNtp = millis();
    timeClient.update();
  }

  String t = getTime();

  display.displayText(t.c_str(), PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  display.displayAnimate();
}

// ================= MATRIX ANIMATIONS =================
#define UNIT_DELAY      25
#define MIDLINE_DELAY   (6 * UNIT_DELAY)
#define SCANNER_DELAY   (2 * UNIT_DELAY)
#define RANDOM_DELAY    (6 * UNIT_DELAY)
#define FADE_DELAY      (8 * UNIT_DELAY)
#define SPECTRUM_DELAY  (4 * UNIT_DELAY)
#define HEARTBEAT_DELAY (1 * UNIT_DELAY)
#define HEARTS_DELAY    (28 * UNIT_DELAY)
#define EYES_DELAY      (20 * UNIT_DELAY)
#define WIPER_DELAY     (1 * UNIT_DELAY)
#define ARROWS_DELAY    (3 * UNIT_DELAY)
#define ARROWR_DELAY    (8 * UNIT_DELAY)
#define INVADER_DELAY   (6 * UNIT_DELAY)
#define PACMAN_DELAY    (4 * UNIT_DELAY)
#define SINE_DELAY      (2 * UNIT_DELAY)

void resetMatrix() {
  MX.control(MD_MAX72XX::INTENSITY, MAX_INTENSITY / 2);
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  MX.clear();
  prevTimeAnim = 0;
}

bool graphicMidline1(bool bInit) {
  if (bInit) {
    resetMatrix();
    bInit = false;
  } else {
    for (uint8_t j = 0; j < MAX_DEVICES; j++) {
      MX.setRow(j, 3, 0xff);
      MX.setRow(j, 4, 0xff);
    }
  }
  return bInit;
}

bool graphicMidline2(bool bInit) {
  static uint8_t idx = 0;
  static int8_t idOffs = 1;
  if (bInit) {
    resetMatrix();
    idx = 0;
    idOffs = 1;
    bInit = false;
  }
  if (millis() - prevTimeAnim < MIDLINE_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (uint8_t j = 0; j < MAX_DEVICES; j++) {
    MX.setRow(j, idx, 0x00);
    MX.setRow(j, ROW_SIZE - 1 - idx, 0x00);
  }
  idx += idOffs;
  if ((idx == 0) || (idx == ROW_SIZE - 1)) idOffs = -idOffs;
  for (uint8_t j = 0; j < MAX_DEVICES; j++) {
    MX.setRow(j, idx, 0xff);
    MX.setRow(j, ROW_SIZE - 1 - idx, 0xff);
  }
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  return bInit;
}

bool graphicScanner(bool bInit) {
  const uint8_t width = 3;
  static uint8_t idx = 0;
  static int8_t idOffs = 1;
  if (bInit) {
    resetMatrix();
    idx = 0;
    idOffs = 1;
    bInit = false;
  }
  if (millis() - prevTimeAnim < SCANNER_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (uint8_t i = 0; i < width; i++) MX.setColumn(idx + i, 0);
  idx += idOffs;
  if ((idx == 0) || (idx + width == MX.getColumnCount())) idOffs = -idOffs;
  for (uint8_t i = 0; i < width; i++) MX.setColumn(idx + i, 0xff);
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  return bInit;
}

bool graphicRandom(bool bInit) {
  if (bInit) {
    resetMatrix();
    bInit = false;
  }
  if (millis() - prevTimeAnim < RANDOM_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (uint8_t i = 0; i < MX.getColumnCount(); i++) MX.setColumn(i, (uint8_t)random(255));
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  return bInit;
}

bool graphicFade(bool bInit) {
  static uint8_t intensity = 0;
  static int8_t iOffs = 1;
  if (bInit) {
    resetMatrix();
    MX.control(MD_MAX72XX::INTENSITY, intensity);
    MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
    for (uint8_t i = 0; i < MX.getColumnCount(); i++) MX.setColumn(i, 0xff);
    MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
    bInit = false;
  }
  if (millis() - prevTimeAnim < FADE_DELAY) return bInit;
  prevTimeAnim = millis();

  intensity += iOffs;
  if ((intensity == 0) || (intensity == MAX_INTENSITY)) iOffs = -iOffs;
  MX.control(MD_MAX72XX::INTENSITY, intensity);
  return bInit;
}

bool graphicSpectrum1(bool bInit) {
  if (bInit) {
    resetMatrix();
    bInit = false;
  }
  if (millis() - prevTimeAnim < SPECTRUM_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    uint8_t r = random(ROW_SIZE);
    uint8_t cd = 0;
    for (uint8_t j = 0; j < r; j++) cd |= 1 << j;
    for (uint8_t j = 1; j < COL_SIZE - 1; j++) MX.setColumn(i, j, ~cd);
  }
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  return bInit;
}

bool graphicHeartbeat(bool bInit) {
  #define BASELINE_ROW 4
  static uint8_t stateLocal;
  static uint8_t r, c;
  static bool bPoint;
  if (bInit) {
    resetMatrix();
    stateLocal = 0;
    r = BASELINE_ROW;
    c = MX.getColumnCount() - 1;
    bPoint = true;
    bInit = false;
  }
  if (millis() - prevTimeAnim < HEARTBEAT_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.setPoint(r, c, bPoint);
  switch (stateLocal) {
    case 0: if (c == MX.getColumnCount() / 2 + COL_SIZE) stateLocal = 1; c--; break;
    case 1: if (r != 0) { r--; c--; } else stateLocal = 2; break;
    case 2: if (r != ROW_SIZE - 1) { r++; c--; } else stateLocal = 3; break;
    case 3: if (r != BASELINE_ROW) { r--; c--; } else stateLocal = 4; break;
    case 4:
      if (c == 0) { c = MX.getColumnCount() - 1; bPoint = !bPoint; stateLocal = 0; }
      else c--;
      break;
    default: stateLocal = 0; break;
  }
  return bInit;
}

bool graphicHearts(bool bInit) {
  #define NUM_HEARTS ((MAX_DEVICES / 2) + 1)
  const uint8_t heartFull[] = {0x1c, 0x3e, 0x7e, 0xfc};
  const uint8_t heartEmpty[] = {0x1c, 0x22, 0x42, 0x84};
  const uint8_t offset = MX.getColumnCount() / (NUM_HEARTS + 1);
  const uint8_t dataSize = (sizeof(heartFull) / sizeof(heartFull[0]));
  static bool bEmpty;
  if (bInit) {
    resetMatrix();
    bEmpty = true;
    bInit = false;
  }
  if (millis() - prevTimeAnim < HEARTS_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (uint8_t h = 1; h <= NUM_HEARTS; h++) {
    for (uint8_t i = 0; i < dataSize; i++) {
      MX.setColumn((h * offset) - dataSize + i, bEmpty ? heartEmpty[i] : heartFull[i]);
      MX.setColumn((h * offset) + dataSize - i - 1, bEmpty ? heartEmpty[i] : heartFull[i]);
    }
  }
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  bEmpty = !bEmpty;
  return bInit;
}

bool graphicEyes(bool bInit) {
  #define NUM_EYES 2
  const uint8_t eyeOpen[] = {0x18, 0x3c, 0x66, 0x66};
  const uint8_t eyeClose[] = {0x18, 0x3c, 0x3c, 0x3c};
  const uint8_t offset = MX.getColumnCount() / (NUM_EYES + 1);
  const uint8_t dataSize = (sizeof(eyeOpen) / sizeof(eyeOpen[0]));
  bool bOpen;
  if (bInit) {
    resetMatrix();
    bInit = false;
  }
  if (millis() - prevTimeAnim < EYES_DELAY) return bInit;
  prevTimeAnim = millis();

  bOpen = (random(1000) > 100);
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (uint8_t e = 1; e <= NUM_EYES; e++) {
    for (uint8_t i = 0; i < dataSize; i++) {
      MX.setColumn((e * offset) - dataSize + i, bOpen ? eyeOpen[i] : eyeClose[i]);
      MX.setColumn((e * offset) + dataSize - i - 1, bOpen ? eyeOpen[i] : eyeClose[i]);
    }
  }
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  return bInit;
}

bool graphicRightWink(bool bInit) {
  const uint8_t eyeOpen[] = {0x18, 0x3c, 0x66, 0x66};
  const uint8_t eyeClose[] = {0x18, 0x3c, 0x3c, 0x3c};
  const uint8_t offset = MX.getColumnCount() / 3;
  const uint8_t dataSize = sizeof(eyeOpen) / sizeof(eyeOpen[0]);
  static bool rightOpen = true;

  if (bInit) {
    resetMatrix();
    rightOpen = true;
    bInit = false;
  }
  if (millis() - prevTimeAnim < EYES_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (uint8_t i = 0; i < dataSize; i++) {
    // Left eye stays open
    MX.setColumn((offset) - dataSize + i, eyeOpen[i]);
    MX.setColumn((offset) + dataSize - i - 1, eyeOpen[i]);
    // Right eye blinks
    MX.setColumn((2 * offset) - dataSize + i, rightOpen ? eyeOpen[i] : eyeClose[i]);
    MX.setColumn((2 * offset) + dataSize - i - 1, rightOpen ? eyeOpen[i] : eyeClose[i]);
  }
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  rightOpen = !rightOpen;
  return bInit;
}

bool graphicLeftWink(bool bInit) {
  const uint8_t eyeOpen[] = {0x18, 0x3c, 0x66, 0x66};
  const uint8_t eyeClose[] = {0x18, 0x3c, 0x3c, 0x3c};
  const uint8_t offset = MX.getColumnCount() / 3;
  const uint8_t dataSize = sizeof(eyeOpen) / sizeof(eyeOpen[0]);
  static bool leftOpen = true;

  if (bInit) {
    resetMatrix();
    leftOpen = true;
    bInit = false;
  }
  if (millis() - prevTimeAnim < EYES_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (uint8_t i = 0; i < dataSize; i++) {
    // Left eye blinks
    MX.setColumn((offset) - dataSize + i, leftOpen ? eyeOpen[i] : eyeClose[i]);
    MX.setColumn((offset) + dataSize - i - 1, leftOpen ? eyeOpen[i] : eyeClose[i]);
    // Right eye stays open
    MX.setColumn((2 * offset) - dataSize + i, eyeOpen[i]);
    MX.setColumn((2 * offset) + dataSize - i - 1, eyeOpen[i]);
  }
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  leftOpen = !leftOpen;
  return bInit;
}

bool graphicBounceBall(bool bInit) {
  static uint8_t idx = 0;
  static int8_t idOffs = 1;
  if (bInit) {
    resetMatrix();
    bInit = false;
  }
  if (millis() - prevTimeAnim < SCANNER_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  MX.setColumn(idx, 0);
  MX.setColumn(idx + 1, 0);
  idx += idOffs;
  if ((idx == 0) || (idx == MX.getColumnCount() - 2)) idOffs = -idOffs;
  MX.setColumn(idx, 0x18);
  MX.setColumn(idx + 1, 0x18);
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  return bInit;
}

bool graphicArrowScroll(bool bInit) {
  const uint8_t arrow[] = {0x3c, 0x66, 0xc3, 0x99};
  const uint8_t dataSize = (sizeof(arrow) / sizeof(arrow[0]));
  static uint8_t idx = 0;
  if (bInit) {
    resetMatrix();
    bInit = false;
  }
  if (millis() - prevTimeAnim < ARROWS_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  MX.transform(MD_MAX72XX::TSL);
  MX.setColumn(0, arrow[idx++]);
  if (idx == dataSize) idx = 0;
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  return bInit;
}

bool graphicScroller(bool bInit) {
  const uint8_t width = 3;
  const uint8_t offset = MX.getColumnCount() / 3;
  static uint8_t idx = 0;
  if (bInit) {
    resetMatrix();
    idx = 0;
    bInit = false;
  }
  if (millis() - prevTimeAnim < SCANNER_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  MX.transform(MD_MAX72XX::TSL);
  MX.setColumn(0, idx < width ? 0xff : 0);
  if (++idx == offset) idx = 0;
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  return bInit;
}

bool graphicWiper(bool bInit) {
  static uint8_t idx = 0;
  static int8_t idOffs = 1;
  if (bInit) {
    resetMatrix();
    bInit = false;
  }
  if (millis() - prevTimeAnim < WIPER_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.setColumn(idx, idOffs == 1 ? 0xff : 0);
  idx += idOffs;
  if ((idx == 0) || (idx == MX.getColumnCount())) idOffs = -idOffs;
  return bInit;
}

bool graphicInvader(bool bInit) {
  const uint8_t invader1[] = {0x0e, 0x98, 0x7d, 0x36, 0x3c};
  const uint8_t invader2[] = {0x70, 0x18, 0x7d, 0xb6, 0x3c};
  const uint8_t dataSize = (sizeof(invader1) / sizeof(invader1[0]));
  static int8_t idx;
  static bool iType;
  if (bInit) {
    resetMatrix();
    bInit = false;
    idx = -dataSize;
    iType = false;
  }
  if (millis() - prevTimeAnim < INVADER_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  MX.clear();
  for (uint8_t i = 0; i < dataSize; i++) {
    MX.setColumn(idx - dataSize + i, iType ? invader1[i] : invader2[i]);
    MX.setColumn(idx + dataSize - i - 1, iType ? invader1[i] : invader2[i]);
  }
  idx++;
  if (idx == MX.getColumnCount() + (dataSize * 2)) bInit = true;
  iType = !iType;
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  return bInit;
}

bool graphicPacman(bool bInit) {
  #define MAX_FRAMES 4
  #define PM_DATA_WIDTH 18
  const uint8_t pacman[MAX_FRAMES][PM_DATA_WIDTH] = {
    {0x3c,0x7e,0x7e,0xff,0xe7,0xc3,0x81,0x00,0x00,0x00,0x00,0xfe,0x7b,0xf3,0x7f,0xfb,0x73,0xfe},
    {0x3c,0x7e,0xff,0xff,0xe7,0xe7,0x42,0x00,0x00,0x00,0x00,0xfe,0x7b,0xf3,0x7f,0xfb,0x73,0xfe},
    {0x3c,0x7e,0xff,0xff,0xff,0xe7,0x66,0x24,0x00,0x00,0x00,0xfe,0x7b,0xf3,0x7f,0xfb,0x73,0xfe},
    {0x3c,0x7e,0xff,0xff,0xff,0xff,0x7e,0x3c,0x00,0x00,0x00,0xfe,0x7b,0xf3,0x7f,0xfb,0x73,0xfe}
  };
  static int16_t idx;
  static uint8_t frame;
  static int8_t deltaFrame;
  if (bInit) {
    resetMatrix();
    bInit = false;
    idx = -1;
    frame = 0;
    deltaFrame = 1;
  }
  if (millis() - prevTimeAnim < PACMAN_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  MX.clear();
  for (uint8_t i = 0; i < PM_DATA_WIDTH; i++) MX.setColumn(idx - PM_DATA_WIDTH + i, 0);
  idx++;
  for (uint8_t i = 0; i < PM_DATA_WIDTH; i++) MX.setColumn(idx - PM_DATA_WIDTH + i, pacman[frame][i]);
  frame += deltaFrame;
  if (frame == 0 || frame == MAX_FRAMES - 1) deltaFrame = -deltaFrame;
  if (idx == MX.getColumnCount() + PM_DATA_WIDTH) bInit = true;
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  return bInit;
}

bool graphicArrowRotate(bool bInit) {
  static uint16_t idx;
  uint8_t arrow[COL_SIZE] = {0,0b00011000,0b00111100,0b01111110,0b00011000,0b00011000,0b00011000,0};
  MD_MAX72XX::transformType_t t[] = {
    MD_MAX72XX::TRC, MD_MAX72XX::TRC,
    MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR, MD_MAX72XX::TSR,
    MD_MAX72XX::TRC, MD_MAX72XX::TRC,
    MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL, MD_MAX72XX::TSL,
    MD_MAX72XX::TRC
  };
  if (bInit) {
    resetMatrix();
    bInit = false;
    idx = 0;
    MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
    for (uint8_t j = 0; j < MX.getDeviceCount(); j++) MX.setBuffer(((j + 1) * COL_SIZE) - 1, COL_SIZE, arrow);
    MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  }
  if (millis() - prevTimeAnim < ARROWR_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::WRAPAROUND, MD_MAX72XX::ON);
  MX.transform(t[idx++]);
  MX.control(MD_MAX72XX::WRAPAROUND, MD_MAX72XX::OFF);
  if (idx == (sizeof(t) / sizeof(t[0]))) bInit = true;
  return bInit;
}

bool graphicSpectrum2(bool bInit) {
  if (bInit) {
    resetMatrix();
    bInit = false;
  }
  if (millis() - prevTimeAnim < SPECTRUM_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (uint8_t i = 0; i < MX.getColumnCount(); i++) {
    uint8_t r = random(ROW_SIZE);
    uint8_t cd = 0;
    for (uint8_t j = 0; j < r; j++) cd |= 1 << j;
    MX.setColumn(i, ~cd);
  }
  MX.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  return bInit;
}

bool graphicSinewave(bool bInit) {
  static uint8_t curWave = 0;
  static uint8_t idx;
  #define SW_DATA_WIDTH 11
  const uint8_t waves[][SW_DATA_WIDTH] = {
    {9, 8, 6, 1, 6, 24, 96, 128, 96, 16, 0},
    {6, 12, 2, 12, 48, 64, 48, 0, 0, 0, 0},
    {10, 12, 2, 1, 2, 12, 48, 64, 128, 64, 48}
  };
  const uint8_t WAVE_COUNT = sizeof(waves) / (SW_DATA_WIDTH * sizeof(uint8_t));
  if (bInit) {
    resetMatrix();
    bInit = false;
    idx = 1;
  }
  if (millis() - prevTimeAnim < SINE_DELAY) return bInit;
  prevTimeAnim = millis();

  MX.control(MD_MAX72XX::WRAPAROUND, MD_MAX72XX::ON);
  MX.transform(MD_MAX72XX::TSL);
  MX.setColumn(0, waves[curWave][idx++]);
  if (idx > waves[curWave][0]) {
    curWave = random(WAVE_COUNT);
    idx = 1;
  }
  MX.control(MD_MAX72XX::WRAPAROUND, MD_MAX72XX::OFF);
  return bInit;
}
