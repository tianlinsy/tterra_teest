// ============================================================
//  Bloom demo — lights the 9 petals up one at a time, as if the
//  wearer were meeting people, then sparkles at full bloom, rests,
//  and starts over. No Wi-Fi, no Firebase: just the LED effect.
//
//  Good for: checking the strip wiring, showing the effect on a
//  table, filming the bracelet.
//
//  Board:   Seeed Studio XIAO ESP32C3
//  Library: Adafruit NeoPixel
// ============================================================

#include <Adafruit_NeoPixel.h>

// ---------------- settings ----------------
#define LED_PIN        D0       // strip DIN (through the 330 Ω resistor)
#define NUM_LEDS       9
#define BRIGHTNESS     40       // 0-255

#define SECONDS_BETWEEN_PEOPLE   3.0   // pause between each new "connection"
#define BLOOM_FADE_MS            600   // how long one petal takes to fade in
#define FULL_HOLD_SECONDS        6     // admire the full flower this long
#define RESET_HOLD_SECONDS       2     // dark pause before starting over

// Plant colour for this run (pick one, same palette as the website)
#define PLANT  CHERRY
// ------------------------------------------

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

enum PlantId { LOTUS, ROSE, SUNFLOWER, LAVENDER, FERN, ORCHID, MAPLE, CHERRY, CACTUS, POPPY, BLUEBELL, JASMINE };
const uint8_t PALETTE[][3] = {
  {0xe4,0x8f,0xb0}, {0xe4,0x57,0x2e}, {0xf4,0xc4,0x30}, {0x9b,0x7e,0xde},
  {0x4c,0x95,0x6c}, {0xc7,0x7d,0xff}, {0xd1,0x49,0x5b}, {0xff,0xb7,0xc5},
  {0x57,0xa7,0x73}, {0xef,0x47,0x6f}, {0x5c,0x7a,0xff}, {0xea,0xe2,0xb7},
};
uint32_t petal;

uint32_t dim(uint32_t c, float f) {
  return strip.Color(((c >> 16) & 0xFF) * f, ((c >> 8) & 0xFF) * f, (c & 0xFF) * f);
}

// Fade one petal in with a slight ease-in so it feels like it's opening.
void bloomPetal(int idx) {
  const int steps = 30;
  for (int s = 0; s <= steps; s++) {
    float t = (float)s / steps;
    float eased = t * t * (3 - 2 * t);          // smoothstep
    strip.setPixelColor(idx, dim(petal, eased));
    strip.show();
    delay(BLOOM_FADE_MS / steps);
  }
}

// Little "someone just tapped" flicker: the newest petal glints white, then settles.
void glint(int idx) {
  for (int k = 0; k < 2; k++) {
    strip.setPixelColor(idx, strip.Color(255, 255, 255)); strip.show(); delay(60);
    strip.setPixelColor(idx, petal);                       strip.show(); delay(90);
  }
}

// Gentle breathing of everything lit, used while "waiting for the next person".
void breatheLit(int litCount, float seconds) {
  unsigned long t0 = millis();
  while (millis() - t0 < seconds * 1000) {
    float phase = (millis() - t0) / 1000.0f;
    float f = 0.75f + 0.25f * sinf(phase * 2.0f);       // 0.5 .. 1.0
    for (int i = 0; i < litCount; i++) strip.setPixelColor(i, dim(petal, f));
    strip.show();
    delay(20);
  }
  for (int i = 0; i < litCount; i++) strip.setPixelColor(i, petal);
  strip.show();
}

// Full-bloom celebration: three white waves around the flower, then a shimmer.
void sparkle() {
  for (int r = 0; r < 3; r++)
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(255, 255, 255)); strip.show(); delay(40);
      strip.setPixelColor(i, petal);                      strip.show();
    }
  for (int k = 0; k < 40; k++) {                           // random twinkle
    int i = random(NUM_LEDS);
    strip.setPixelColor(i, strip.Color(255, 255, 255)); strip.show(); delay(30);
    strip.setPixelColor(i, petal);                      strip.show(); delay(40);
  }
}

// Petals close from the outside in.
void wilt() {
  for (int i = NUM_LEDS - 1; i >= 0; i--) {
    for (int s = 20; s >= 0; s--) { strip.setPixelColor(i, dim(petal, s / 20.0f)); strip.show(); delay(12); }
  }
}

void setup() {
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear(); strip.show();
  petal = strip.Color(PALETTE[PLANT][0], PALETTE[PLANT][1], PALETTE[PLANT][2]);
  randomSeed(analogRead(A0));
}

void loop() {
  // Seed: dark, a single faint pulse on the first petal ("waiting to meet someone")
  breatheLit(0, 1.0);

  // Meet people one at a time
  for (int person = 0; person < NUM_LEDS; person++) {
    bloomPetal(person);
    glint(person);
    if (person < NUM_LEDS - 1) breatheLit(person + 1, SECONDS_BETWEEN_PEOPLE);
  }

  // Full bloom
  sparkle();
  breatheLit(NUM_LEDS, FULL_HOLD_SECONDS);

  // Start over
  wilt();
  strip.clear(); strip.show();
  delay(RESET_HOLD_SECONDS * 1000);
}
