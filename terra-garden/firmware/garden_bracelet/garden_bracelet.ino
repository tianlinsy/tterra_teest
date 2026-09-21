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
//  Edit config.h — nothing in this file needs changing.
//
//  Board:     Seeed Studio XIAO ESP32C3   (esp32 core by Espressif)
//  Libraries: Adafruit NeoPixel, ArduinoJson (v7)   — both in Library Manager
// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

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
bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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
    JsonDocument doc;                       // ArduinoJson v7
    DeserializationError err = deserializeJson(doc, http.getString());
    if (err) { Serial.printf("JSON error: %s\n", err.c_str()); code = -2; }
    else {
      // Firestore encodes integers as strings: {"fields":{"count":{"integerValue":"3"},...}}
      const char* cnt = doc["fields"]["count"]["integerValue"] | "0";
      const char* pl  = doc["fields"]["plant"]["stringValue"]  | "";
      count = atoi(cnt);
      strncpy(plant, pl, plantLen - 1); plant[plantLen - 1] = 0;
    }
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
