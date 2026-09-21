// ============================================================
//  The Garden bracelet — Seeed XIAO ESP32-C3 firmware
//
//  Reads its owner's guest document from Firestore over Wi-Fi and
//  lights one LED per connection, in the owner's plant colour.
//
//    GET https://firestore.googleapis.com/v1/projects/<PROJECT>/databases/(default)
//        /documents/guests/<BRACELET_ID>?mask.fieldPaths=count&mask.fieldPaths=plant
//
//  No login is needed: firebase/firestore.rules allow public reads of guests.
//
//  Edit the SETTINGS block below; nothing else needs changing.
//
//  Board:     Seeed Studio XIAO ESP32C3   (esp32 core by Espressif)
//  Library:   Adafruit NeoPixel   (Tools → Manage Libraries)
// ============================================================

// ============================================================
//  SETTINGS — the only part you edit
// ============================================================

// --- This bracelet's code. Printed inside the bracelet, written on its NFC
//     tag as  https://YOUR-SITE/?met=A1  and typed by its owner when they
//     "plant themselves". Uppercase letters, digits, dashes; max 16 chars.
#define BRACELET_ID          "A1"

// --- Wi-Fi: Alex's iPhone Personal Hotspot ("Maximize Compatibility" must be ON).
//     iPhone hotspot names contain a curly apostrophe, so the firmware also
//     accepts any network whose name STARTS WITH WIFI_SSID_PREFIX.
#define WIFI_SSID            "Alex\xe2\x80\x99s iPhone (3)"   // exact name (curly ’)
#define WIFI_SSID_PREFIX     "Alex"                         // fallback: first network starting with this
#define WIFI_PASSWORD        "2444666668888888"

// --- Firebase → Project settings → General
#define FIREBASE_PROJECT_ID  "terra-garden-f8f03"
#define FIREBASE_API_KEY     "AIzaSyDDr518HFVbY1-uyTdHRyiw7eh43KOxdAk"     // same apiKey as web/firebase-config.js

// --- LED strip (WS2812B, 9 LEDs = 9 connections to full bloom)
#define LED_PIN              D0      // XIAO ESP32-C3 pin D0 (GPIO2) -> strip DIN
#define NUM_LEDS             9       // MUST equal BLOOM_AT in web/firebase-config.js
#define BRIGHTNESS           40      // 0-255. 40 is plenty on a wrist

// --- Timing
#define POLL_INTERVAL_MS     3000    // how often to ask Firestore for the count
#define WIFI_TIMEOUT_MS      20000
#define HTTP_TIMEOUT_MS      8000

// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Plant palette — must match PLANTS in web/app.js
struct Plant { const char* id; uint8_t r, g, b; };
const Plant PLANTS[] = {
  {"lotus",     0xe4, 0x8f, 0xb0}, {"rose",      0xe4, 0x57, 0x2e},
  {"sunflower", 0xf4, 0xc4, 0x30}, {"lavender",  0x9b, 0x7e, 0xde},
  {"fern",      0x4c, 0x95, 0x6c}, {"orchid",    0xc7, 0x7d, 0xff},
  {"maple",     0xd1, 0x49, 0x5b}, {"cherry",    0xff, 0xb7, 0xc5},
  {"cactus",    0x57, 0xa7, 0x73}, {"poppy",     0xef, 0x47, 0x6f},
  {"bluebell",  0x5c, 0x7a, 0xff}, {"jasmine",   0xea, 0xe2, 0xb7},
};

enum State { UNKNOWN, UNCLAIMED, CLAIMED };

State         state       = UNKNOWN;
int           shownCount  = -1;             // what's on the strip now
uint32_t      petalColor;                   // current plant colour
unsigned long lastPoll    = 0;
int           failStreak  = 0;
bool          celebrated  = false;

void sparkle();
void drawCount(int n);

// ------------------------------------------------------------ colours
uint32_t dim(uint32_t c, float f) {
  return strip.Color(((c >> 16) & 0xFF) * f, ((c >> 8) & 0xFF) * f, (c & 0xFF) * f);
}
uint32_t colorForPlant(const char* id) {
  for (const Plant& p : PLANTS) if (id && strcmp(p.id, id) == 0) return strip.Color(p.r, p.g, p.b);
  return strip.Color(0xf4, 0xa6, 0xc6);     // default --bloom pink
}

// ------------------------------------------------------------ drawing
void drawCount(int n) {
  for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, i < n ? petalColor : 0);
  strip.show();
}
void bloomPetal(int idx) {                  // fade one LED from dark to full
  for (int s = 0; s <= 20; s++) { strip.setPixelColor(idx, dim(petalColor, s / 20.0f)); strip.show(); delay(25); }
}
void animateTo(int n) {
  if (shownCount < 0)        { drawCount(0); for (int i = 0; i < n; i++) bloomPetal(i); }
  else if (n > shownCount)   { for (int i = shownCount; i < n; i++) bloomPetal(i); }
  else if (n < shownCount)   { drawCount(n); }
  shownCount = n;
  if (n >= NUM_LEDS && !celebrated) { sparkle(); celebrated = true; }
  if (n <  NUM_LEDS) celebrated = false;
}
void sparkle() {                            // full bloom celebration
  for (int r = 0; r < 3; r++)
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(255, 255, 255)); strip.show(); delay(35);
      strip.setPixelColor(i, petalColor);                 strip.show();
    }
  drawCount(NUM_LEDS);
}
void breathe(uint32_t c) {                  // soft pulse on LED 0
  static int lvl = 0, dir = 1;
  lvl += dir * 4; if (lvl >= 100 || lvl <= 0) dir = -dir;
  strip.setPixelColor(0, dim(c, lvl / 100.0f)); strip.show(); delay(20);
}
void blinkError() {                         // red double-blink on LED 0
  for (int k = 0; k < 2; k++) {
    strip.setPixelColor(0, strip.Color(255, 0, 0)); strip.show(); delay(80);
    strip.setPixelColor(0, 0);                        strip.show(); delay(80);
  }
  if (shownCount > 0) strip.setPixelColor(0, petalColor);
  strip.show();
}

// ------------------------------------------------------------ Wi-Fi
// Find the hotspot: exact name first, else the strongest network whose
// name starts with WIFI_SSID_PREFIX (handles iPhone's curly apostrophe).
String pickSSID() {
  int n = WiFi.scanNetworks();
  String best = ""; int bestRssi = -999;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s == WIFI_SSID) { WiFi.scanDelete(); return s; }
    if (s.startsWith(WIFI_SSID_PREFIX) && WiFi.RSSI(i) > bestRssi) { best = s; bestRssi = WiFi.RSSI(i); }
  }
  WiFi.scanDelete();
  return best.length() ? best : String(WIFI_SSID);
}

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  String ssid = pickSSID();
  Serial.printf("Connecting to \"%s\"", ssid.c_str());
  WiFi.begin(ssid.c_str(), WIFI_PASSWORD);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) {
    breathe(strip.Color(40, 80, 255));      // blue = connecting
    if ((millis() - t0) % 500 < 25) Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected, IP %s\n", WiFi.localIP().toString().c_str());
    WiFi.setSleep(true);                    // modem sleep between packets
    strip.setPixelColor(0, 0); strip.show();
    return true;
  }
  Serial.println("\nWi-Fi timeout");
  return false;
}

// ------------------------------------------------------------ Firestore
// Pull the quoted value that follows  key  in a JSON string, e.g.
//   extractField(body, "\"integerValue\"")  ->  "3"
String extractField(const String& body, const char* key) {
  int k = body.indexOf(key);            if (k < 0) return "";
  int q1 = body.indexOf('"', k + strlen(key)); if (q1 < 0) return "";
  int q2 = body.indexOf('"', q1 + 1);   if (q2 < 0) return "";
  return body.substring(q1 + 1, q2);
}

// Returns HTTP status; on 200 fills count + plant.
int fetchGuest(int& count, char* plant, size_t plantLen) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);

  String url = String("https://firestore.googleapis.com/v1/projects/") + FIREBASE_PROJECT_ID +
               "/databases/(default)/documents/guests/" + BRACELET_ID +
               "?mask.fieldPaths=count&mask.fieldPaths=plant&key=" + FIREBASE_API_KEY;
  if (!http.begin(client, url)) return -1;

  int code = http.GET();
  if (code == 200) {
    // Firestore reply looks like:
    //   {"fields":{"count":{"integerValue":"3"},"plant":{"stringValue":"fern"}},...}
    String body = http.getString();
    count = extractField(body, "\"integerValue\"").toInt();
    String pl = extractField(body, "\"stringValue\"");
    strncpy(plant, pl.c_str(), plantLen - 1); plant[plantLen - 1] = 0;
  } else if (code != 404) {
    Serial.printf("HTTP %d: %s\n", code, http.errorToString(code).c_str());
  }
  http.end();
  return code;
}

// ------------------------------------------------------------ main
void setup() {
  Serial.begin(115200);
  delay(200);
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear(); strip.show();
  petalColor = colorForPlant("");
  Serial.println("\nThe Garden bracelet " BRACELET_ID);
  connectWiFi();
}

void loop() {
  if (!connectWiFi()) { delay(2000); return; }

  // Unclaimed bracelet: amber breathing until someone plants themselves with this code.
  if (state == UNCLAIMED) breathe(strip.Color(255, 140, 0));

  if (millis() - lastPoll >= POLL_INTERVAL_MS || state == UNKNOWN) {
    lastPoll = millis();
    int n = 0; char plant[24] = "";
    int code = fetchGuest(n, plant, sizeof plant);

    if (code == 404) {                                    // nobody has claimed this code yet
      if (state != UNCLAIMED) { Serial.println("Bracelet not planted yet"); strip.clear(); strip.show(); shownCount = -1; }
      state = UNCLAIMED; failStreak = 0;
      return;
    }
    if (code != 200) {
      failStreak++;
      Serial.printf("Fetch failed (%d in a row)\n", failStreak);
      blinkError();
      if (failStreak >= 5) { WiFi.disconnect(); failStreak = 0; }
      return;
    }

    failStreak = 0;
    uint32_t c = colorForPlant(plant);
    if (state != CLAIMED || c != petalColor) {            // first sight, or plant changed (reset + replant)
      petalColor = c; state = CLAIMED; shownCount = -1;
      Serial.printf("Planted as %s\n", plant);
    }
    n = constrain(n, 0, NUM_LEDS);
    if (n != shownCount) { Serial.printf("Connections: %d\n", n); animateTo(n); }
  }
  delay(state == UNCLAIMED ? 0 : 50);
}
