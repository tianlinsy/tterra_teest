// ============================================================
//  Hardware test — LED strip + battery. No Wi-Fi, no website.
//
//  What it does, over and over:
//    1. Prints the battery voltage to Serial Monitor (if the divider is wired).
//    2. Tries driving the LED strip from each data pin in turn
//       (D0, D1, D2 ...). For each pin it flashes the whole strip
//       RED -> GREEN -> BLUE, then chases one white LED along it.
//       Serial Monitor says which pin is being tried, so if the
//       strip only lights on, say, "D2", your data wire is on D2.
//
//  Board:   XIAO_ESP32C3   (Tools -> USB CDC On Boot -> Enabled,
//                           or Serial Monitor stays blank)
//  Library: Adafruit NeoPixel
//  Serial Monitor: 115200 baud
// ============================================================

#include <Adafruit_NeoPixel.h>

// ---------------- settings ----------------
#define NUM_LEDS     9
#define BRIGHTNESS   30          // low on purpose: easy on a small battery

// Try every pin, or only D0 (the one the real firmware uses)?
#define SCAN_ALL_PINS  true

// Battery voltage. The XIAO ESP32-C3 has NO built-in battery sense, so this
// only works if you add two equal resistors (e.g. 2 x 220 kOhm):
//     BAT+ --[220k]--+--[220k]-- GND
//                    |
//                   A1 (D1)
// With nothing wired to A1 the reading is meaningless and is reported as such.
#define BATTERY_PIN      A1
#define BATTERY_DIVIDER  2.0     // equal resistors halve the voltage
// ------------------------------------------

struct TestPin { int pin; const char* name; };
const TestPin PINS[] = {
  {D0, "D0"}, {D2, "D2"}, {D3, "D3"}, {D4, "D4"}, {D5, "D5"},
  {D6, "D6"}, {D7, "D7"}, {D8, "D8"}, {D9, "D9"}, {D10, "D10"},
  // D1 is left out: it's the battery-sense pin above.
};
const int NUM_PINS = sizeof(PINS) / sizeof(PINS[0]);

Adafruit_NeoPixel strip(NUM_LEDS, D0, NEO_GRB + NEO_KHZ800);

void fill(uint32_t c) { for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, c); strip.show(); }

void reportBattery() {
  analogReadResolution(12);
  uint32_t mv = 0;
  for (int i = 0; i < 16; i++) mv += analogReadMilliVolts(BATTERY_PIN);
  float v = (mv / 16.0f) * BATTERY_DIVIDER / 1000.0f;

  Serial.printf("Battery: %.2f V  ", v);
  if (v < 1.0f)       Serial.println("(no divider on A1 - can't measure; see settings)");
  else if (v < 3.3f)  Serial.println("(LOW - charge it; strip may flicker or not light)");
  else if (v < 3.7f)  Serial.println("(okay, getting low)");
  else if (v <= 4.3f) Serial.println("(good)");
  else                Serial.println("(above 4.3 V - probably reading USB, not the battery)");
}

void testOnPin(const TestPin& p) {
  Serial.printf("Driving strip from %s ... watch the LEDs\n", p.name);
  strip.setPin(p.pin);
  strip.begin();
  strip.setBrightness(BRIGHTNESS);

  fill(strip.Color(255, 0, 0));   delay(500);
  fill(strip.Color(0, 255, 0));   delay(500);
  fill(strip.Color(0, 0, 255));   delay(500);
  fill(0);
  for (int i = 0; i < NUM_LEDS; i++) {             // white chase: checks every LED
    strip.setPixelColor(i, strip.Color(255, 255, 255)); strip.show(); delay(120);
    strip.setPixelColor(i, 0);
  }
  strip.show();

  pinMode(p.pin, INPUT);                            // release the pin before trying the next
  delay(300);
}

void setup() {
  Serial.begin(115200);
  delay(2000);                                      // time to open Serial Monitor
  Serial.println("\n=== Garden bracelet hardware test ===");
  Serial.printf("%d LEDs, brightness %d\n", NUM_LEDS, BRIGHTNESS);
}

void loop() {
  Serial.println("\n----------------------------------------");
  reportBattery();
  if (SCAN_ALL_PINS) for (int k = 0; k < NUM_PINS; k++) testOnPin(PINS[k]);
  else               testOnPin(PINS[0]);
  Serial.println("Cycle done. If nothing ever lit: check strip power (5V/GND pads),");
  Serial.println("that data goes into DIN (arrows point away), and a shared GND.");
  delay(1500);
}
