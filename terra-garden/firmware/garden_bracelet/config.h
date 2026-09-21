// ============================================================
//  The Garden bracelet — per-bracelet settings
//  This is the ONLY file you need to edit before flashing.
// ============================================================
#pragma once

// --- This bracelet's code. Printed inside the bracelet, written on its NFC
//     tag as  https://YOUR-SITE/?met=A1  and typed by its owner when they
//     "plant themselves". Uppercase letters, digits, dashes; max 16 chars.
#define BRACELET_ID          "A1"

// --- Wi-Fi the bracelet should join (2.4 GHz; a phone hotspot is ideal).
#define WIFI_SSID            "your-hotspot-name"
#define WIFI_PASSWORD        "your-hotspot-password"

// --- Firebase → Project settings → General
#define FIREBASE_PROJECT_ID  "YOUR-PROJECT-ID"
#define FIREBASE_API_KEY     "YOUR-WEB-API-KEY"     // same apiKey as web/firebase-config.js

// --- LED strip (WS2812B, 9 LEDs = 9 connections to full bloom)
#define LED_PIN              D0      // XIAO ESP32-C3 pin D0 (GPIO2) -> strip DIN
#define NUM_LEDS             9       // MUST equal BLOOM_AT in web/firebase-config.js
#define BRIGHTNESS           40      // 0-255. 40 is plenty on a wrist

// --- Timing
#define POLL_INTERVAL_MS     3000    // how often to ask Firestore for the count
#define WIFI_TIMEOUT_MS      20000
#define HTTP_TIMEOUT_MS      8000
