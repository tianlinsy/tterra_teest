# The Garden — Setup Guide

Every guest wears a bracelet with a 9-LED flower and an NFC tag. Tap your phone on someone's bracelet and you've "met" them: both of you gain a connection, both flowers grow on screen, and both bracelets light one more LED. Nine connections is full bloom.

```
   Alex's phone taps Daniel's bracelet (NFC tag → https://SITE/?met=D4)
                    │
                    ▼
        The Garden web page (Alex is A1, signed in on this phone)
                    │  records the connection for BOTH: A1↔D4
                    ▼
               Firestore  guests/A1.count = 3    guests/D4.count = 5
                    ▲                                  ▲
        every 3 s   │ "how many for A1?"               │ "how many for D4?"
                    │                                  │
             Alex's bracelet                     Daniel's bracelet
             (ESP32, Wi-Fi)  → lights 3 LEDs     (ESP32, Wi-Fi)  → lights 5 LEDs
```

Nothing is wired to a computer. Each bracelet runs on its own LiPo and only ever talks to Firestore over Wi-Fi.

## What's in this folder

| Path | What it is |
|---|---|
| `web/index.html`, `web/style.css` | The Garden front page from the repo (Daniel's page), with one new field: **bracelet code**. |
| `web/app.js` | The repo's JS with the `⇢ FIREBASE` hooks filled in. Same UX; data now lives in Firestore. |
| `web/firebase-config.js` | Your Firebase keys + `BLOOM_AT` (9). **Edit this.** |
| `firebase/firestore.rules` | Security rules. Paste into the Firebase console once. |
| `firebase.json`, `.firebaserc` | Firebase Hosting + emulator config (optional; Netlify works too). |
| `firmware/garden_bracelet/config.h` | Wi-Fi, Firebase project, bracelet code. **The only firmware file you edit.** |
| `firmware/garden_bracelet/garden_bracelet.ino` | The ESP32 program. |

Suggested place in the repo: move the site out of `.idea/` (JetBrains treats that folder as IDE settings) into `web/` at the repo root, and add `firmware/` and `firebase/` next to it. Sophie's page can live in `web/sophie/` later and share `firebase-config.js`.

---

## 1. Parts (per bracelet)

| Part | Notes |
|---|---|
| Seeed Studio XIAO ESP32-C3 | Wi-Fi + built-in LiPo charger. |
| WS2812B-style addressable LED strip, 9 LEDs | Three pads per LED: **5V / DIN / GND**. |
| 3.7 V LiPo, 500–1000 mAh | JST connector, optional slide switch. |
| 330 Ω resistor | In series with the data line. |
| 470–1000 µF electrolytic capacitor (≥6.3 V) | Across the strip's power pads. |
| NTAG213/215 NFC sticker | Holds `https://YOUR-SITE/?met=<CODE>`. |
| A printed code (A1, B2, …) | Written inside the bracelet so the owner can type it. |

**You do not need the LM2596S buck converter.** It only steps voltage *down*, and the strip runs fine straight from the LiPo (3.7–4.2 V). At that supply the ESP32's 3.3 V data signal is read correctly, so no level shifter either.

---

## 2. Wiring — two power branches, one data wire

The battery feeds the XIAO and the LED strip **separately**. The strip never draws power through the XIAO; the only connection between them is the data line and the shared ground.

```
                         ┌──────────────────────────────►  XIAO  BAT+  (pad on underside)
      LiPo  +  ──────────┤
                         └──────────────────────────────►  STRIP  5V / VCC
                                                                  ║
                                                            [470–1000 µF]   (+ leg to 5V, stripe to GND)
                                                                  ║
                         ┌──────────────────────────────►  STRIP  GND
      LiPo  −  ──────────┤
                         └──────────────────────────────►  XIAO  BAT−  (pad on underside)

      XIAO  D0  ───[330 Ω]─────────────────────────────►  STRIP  DIN / DI   (the input end — arrows point away)
```

1. Solder battery **+** to the XIAO's **BAT+** pad and **−** to **BAT−** (both on the underside). Put a switch in the + lead if you want on/off.
2. Run a second pair of wires from the battery to the strip's **5V** and **GND**. Not from the XIAO's 5V pin — that pin is only live on USB.
3. Solder the 330 Ω resistor between **D0** and the strip's **DIN**.
4. Solder the capacitor across the strip's 5V and GND pads, long leg to 5V.
5. Cut the strip to 9 LEDs (or set `NUM_LEDS` and `BLOOM_AT` to whatever you have — they must match).

USB-C into the XIAO charges the battery (~350 mA) and the bracelet keeps working while charging.

---

## 3. Firebase project (once, for the whole garden)

1. Go to <https://console.firebase.google.com> → **Add project**. Name it (e.g. `terra-garden`), Analytics off is fine.
2. Left sidebar → **Build → Firestore Database → Create database**. Pick a region near the event. Choose **Start in production mode** (we'll paste our own rules next).
3. Open the **Rules** tab, delete what's there, paste the entire contents of `firebase/firestore.rules`, and click **Publish**.
4. **Project settings** (gear icon) → **General** → scroll to **Your apps** → click the **`</>`** (Web) icon → nickname `garden` → **Register app**. Copy the `firebaseConfig` object it shows.
5. Paste those values into `web/firebase-config.js`. You'll also need two of them for the firmware: `projectId` and `apiKey`.

The API key is not a secret — it identifies the project, and the rules decide what's allowed. With these rules anyone can read guest profiles (the page must show whose bracelet you tapped) and record connections, but a profile can only be created once per bracelet code, and the only field that can change afterwards is the connection count.

---

## 4. Host the website

**Option A — Netlify Drop (fastest):** open <https://app.netlify.com/drop> and drag the `web` folder onto it. You get a URL like `https://xyz.netlify.app` in seconds; rename it under Site settings → Domain.

**Option B — Firebase Hosting:** `npm i -g firebase-tools`, `firebase login`, put your project id in `.firebaserc`, then from this folder run `firebase deploy`. Your site is `https://YOUR-PROJECT-ID.web.app`. (`firebase deploy` also publishes the rules file, so step 3.3 is optional if you use this route.)

Test it: open `https://YOUR-SITE/?met=A1` in a phone browser. You should see the onboarding form with `A1` pre-filled and the hint "Tapped your own bracelet?". Plant yourself, then check **Firestore → Data** in the console: `guests/A1` exists with `count: 0`.

---

## 5. Program the NFC tags

For each bracelet, with the free **NFC Tools** app (iOS/Android):

1. **Write → Add a record → URL/URI** → `https://YOUR-SITE/?met=A1` (that bracelet's code).
2. **Write**, hold the phone on the sticker until it confirms.
3. Optional: **Other → Lock tag** so it can't be rewritten (permanent).
4. Stick it where it isn't covered by the battery or the strip (metal weakens NFC).
5. Write the same code (A1) on the inside of the bracelet so its owner can type it.

iPhone XR+ and NFC Androids open the link automatically — no app needed.

---

## 6. Flash each bracelet

### One-time computer setup

1. Install **Arduino IDE 2.x** — <https://www.arduino.cc/en/software>.
2. **File → Preferences → Additional boards manager URLs**, add
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. **Tools → Board → Boards Manager** → search `esp32` → install **esp32 by Espressif Systems**.
4. **Tools → Manage Libraries** → install **Adafruit NeoPixel** and **ArduinoJson** (by Benoit Blanchon, v7).

### Per bracelet

1. Open `firmware/garden_bracelet/garden_bracelet.ino`; `config.h` opens as a tab.
2. In `config.h` set `BRACELET_ID` (the code on the NFC tag), `WIFI_SSID`/`WIFI_PASSWORD`, `FIREBASE_PROJECT_ID`, `FIREBASE_API_KEY`.
3. USB-C in. **Tools → Board → esp32 → XIAO_ESP32C3**, pick the **Port**.
4. **Upload**. If it can't connect: hold **B** (boot), tap **R** (reset), release **B**, upload again.
5. **Serial Monitor** at 115200: `Connecting to … Connected … Bracelet not planted yet` (amber breathing) until its owner plants themselves, then `Planted as fern`, `Connections: 0`.

### Wi-Fi at the event

Use a phone hotspot with a fixed name/password baked into `config.h`; all bracelets can share one. iPhone: keep **Maximize Compatibility** on (ESP32 is 2.4 GHz only). Networks with a login page (hotel/café) won't work.

---

## 7. Demo-day flow

1. Hand each guest a bracelet. They tap **their own** bracelet → the page opens with their code pre-filled → they enter name, favourite place, photo → **Enter the Garden**. Their bracelet switches from amber breathing to their plant colour (all dark, count 0).
2. Guests tap each other's bracelets. Each tap records the connection for both people; both pages update live, both bracelets bloom one more LED within ~3 s.
3. At 9 connections: Full Bloom on screen, and the bracelet does a white sparkle wave.
4. **Demo mode** at the bottom lists everyone who has planted themselves, for taps without reaching a wrist.
5. **Reset my garden** removes that guest everywhere (their connections, their entries in other people's lists, their profile) and frees the bracelet code for re-use.

---

## 8. What the bracelet's lights mean

| Pattern | Meaning |
|---|---|
| LED 1 breathing blue | Connecting to Wi-Fi |
| LED 1 breathing amber | Online, but nobody has planted themselves with this code yet |
| LEDs bloom in one by one, in a colour | Owner found; that colour is their plant; that's their connection count |
| One more LED fades in | Someone just met them (or they met someone) |
| All 9 lit + white sparkle wave | Full bloom |
| LED 1 double-blinks red | Request to Firestore failed; retries every 3 s, reconnects Wi-Fi after 5 failures |

---

## 9. Testing locally (optional)

`firebase emulators:start` (needs Java) runs Firestore + hosting on your laptop. Open `http://localhost:5000/?emulator&met=A1` — the `?emulator` flag points the page at the local database, so you can rehearse the whole flow without touching the real project. The bracelet firmware always talks to the real project.

---

## 10. Troubleshooting

| Symptom | Fix |
|---|---|
| Page: "Could not reach the garden" | `firebase-config.js` still has placeholders, or rules weren't published. Browser console (F12) shows `permission-denied` or `invalid-api-key`. |
| Page: "Bracelet X hasn't been planted yet" | The person you tapped hasn't onboarded, or their tag was written with a different code than they typed. Codes are case-insensitive but must match. |
| Bracelet stays amber after its owner planted | `BRACELET_ID` in `config.h` ≠ the code they typed. Check Firestore → Data → `guests`. |
| Red double-blink; Serial says `HTTP 403` | Rules not published, or you edited them and broke `allow read`. |
| Red double-blink; Serial says `HTTP 400`/`404` for every bracelet | `FIREBASE_PROJECT_ID` typo (404 on a *known-planted* code means wrong project). |
| Red double-blink; `-1` | Wi-Fi has no internet (hotspot data off, captive portal). |
| Blue breathing never stops | Wrong SSID/password or 5 GHz-only network. |
| Colours wrong / flicker | Add the capacitor; try `NEO_RGB` instead of `NEO_GRB` in the `.ino`. |
| Someone claimed the wrong code | They tap **Reset my garden** and plant again with the right code. |
| Want stricter security (post-demo) | Add Firebase Anonymous Auth, store `owner: request.auth.uid` on the guest doc, and restrict update/delete to the owner. The ESP32 reads stay unauthenticated (public read on `guests`). |
