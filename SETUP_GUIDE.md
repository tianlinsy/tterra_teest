# The Garden — Setup Guide

Every guest has a flower that grows as they meet people. Meeting someone means opening their **personal link** — by tapping the NFC tag on their bracelet, scanning the QR code on their phone, or just following the link they sent you. Both of you gain a connection, both flowers grow on screen, and if either of you is wearing a bracelet, one more LED lights up. Nine connections is full bloom.

```
   Alex opens Daniel's link  (NFC tag / QR / shared URL → https://SITE/?met=D4)
                    │
                    ▼
        The Garden web page  (Alex is A1 on this phone)
                    │   records the connection for BOTH: A1 ↔ D4
                    ▼
               Firestore   guests/A1.count = 3      guests/D4.count = 5
                    ▲                                    ▲
        every 3 s   │ "how many for A1?"                 │ "how many for D4?"
                    │                                    │
             Alex's bracelet                       Daniel's bracelet
             (ESP32, Wi-Fi) → 3 LEDs lit           (ESP32, Wi-Fi) → 5 LEDs lit
```

Bracelets are optional. A guest without one still gets a code, a link and a QR — their flower just lives on their phone. Nothing is wired to a computer; each bracelet runs on its own LiPo and only ever talks to Firestore over Wi-Fi.

**Live site:** <https://terra-garden-f8f03.web.app> · **Firebase project:** `terra-garden-f8f03`

## Repo layout

| Path | What it is |
|---|---|
| `web/` | The Garden site: `index.html`, `style.css`, `app.js` (Firestore-backed), `tags.html` (NFC tag batch sheet), `qrcode.js` (vendored QR library), `firebase-config.js` (project keys + `BLOOM_AT`). |
| `firebase/firestore.rules` | Security rules (already published to the live project). |
| `firebase.json`, `.firebaserc` | Firebase Hosting + emulator config. |
| `firmware/garden_bracelet/garden_bracelet.ino` | The bracelet firmware. Single file; edit the **SETTINGS** block at the top. |
| `firmware/bloom_demo/bloom_demo.ino` | Standalone LED demo (no Wi-Fi) — the "meeting people" animation on a loop. |
| `src/webpages_individual/` | The original hackathon page mock-ups (localStorage demo). Superseded by `web/`. |

---

## 1. How a person gets their link

When someone **plants themselves** on the site they get a code. Two ways:

- **They have a bracelet** → they type the code printed inside it (e.g. `A1`). The bracelet's firmware carries the same code, so the LEDs follow that person.
- **No bracelet** → they leave the code blank and get a random 6-character code (like `P6N2TG`).

Either way, their personal link is `https://terra-garden-f8f03.web.app/?met=<CODE>`, and the site shows it in a **Your link** card with a QR code, **Copy link** and **Share…** buttons. Tapping the QR blows it up full-screen so someone can scan it with their camera. Anyone who opens the link:

- if they're already in the garden → instantly connected to you (both counts go up);
- if they're new → the sign-up form shows "You tapped Daniel's bracelet — plant yourself and they'll be your first connection", and the connection is recorded the moment they finish.

Opening your own link, or someone's link twice, does nothing (it says so).

---

## 2. Parts (per bracelet)

| Part | Notes |
|---|---|
| Seeed Studio XIAO ESP32-C3 | Wi-Fi + built-in LiPo charger. |
| WS2812B-style addressable LED strip, 9 LEDs | Three pads per LED: **5V / DIN / GND**. |
| 3.7 V LiPo, 500–1000 mAh | JST connector, optional slide switch. |
| 330 Ω resistor | In series with the data line. |
| 470–1000 µF electrolytic capacitor (≥6.3 V) | Across the strip's power pads. |
| NTAG213/215 NFC sticker | Holds the bracelet's link. |
| A printed code (A1, B2, …) | Written inside the bracelet so the owner can type it. |

**You do not need a buck/boost converter.** The strip runs straight from the LiPo (3.7–4.2 V), and at that supply the ESP32's 3.3 V data signal is read correctly, so no level shifter either.

---

## 3. Wiring — two power branches, one data wire

The battery feeds the XIAO and the LED strip **separately**. The strip never draws power through the XIAO; the only link between them is the data line and the shared ground.

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

1. Battery **+** → XIAO **BAT+**, **−** → **BAT−** (both pads on the underside). A switch in the + lead if you want on/off.
2. A second pair of wires from the battery to the strip's **5V** and **GND**. Not from the XIAO's 5V pin — that's only live on USB.
3. 330 Ω resistor between **D0** and the strip's **DIN**.
4. Capacitor across the strip's 5V/GND pads, long leg to 5V. It's a local reservoir for the strip's fast current spikes so the first LED doesn't glitch or die on power-up.
5. Cut the strip to 9 LEDs (or set `NUM_LEDS` in the firmware and `BLOOM_AT` in `web/firebase-config.js` to match).

USB-C into the XIAO charges the battery (~350 mA) and the bracelet keeps working while charging.

---

## 4. Firebase (already done for `terra-garden-f8f03`)

The live project is set up: Firestore in `nam5`, rules published, web app registered, keys in `web/firebase-config.js` and the firmware. To recreate it from scratch on another account:

1. <https://console.firebase.google.com> → **Add project**.
2. **Build → Firestore Database → Create database** → production mode.
3. **Rules** tab → paste `firebase/firestore.rules` → **Publish**.
4. **Project settings → General → Your apps → `</>`** → register a web app → copy its config into `web/firebase-config.js`. Put `projectId` and `apiKey` in the firmware SETTINGS too.

The API key is not a secret — it identifies the project; the rules decide what's allowed. Anyone can read guest profiles (the page has to show whose link you opened) and record connections, but a code can only be claimed once, and the only field that changes afterwards is the connection count.

---

## 5. Deploy the website

From the repo root, in a terminal:

```bash
npx -y firebase-tools login          # once per computer — opens a browser
npx -y firebase-tools deploy --only hosting,firestore --project terra-garden-f8f03
```

That pushes `web/` to <https://terra-garden-f8f03.web.app> and re-publishes the rules. Any edit to `web/` needs a redeploy (about a minute).

Smoke test: open `https://terra-garden-f8f03.web.app/?met=A1` on a phone. If `A1` is unclaimed you'll see the form with `A1` pre-filled; plant yourself and check **Firestore → Data** in the console.

---

## 6. NFC tags — one tag per person

Every tag carries **one person's link**. The tag *is* their identity in the garden:

1. Open <https://terra-garden-f8f03.web.app/tags.html>, pick a prefix and how many (e.g. `T01`–`T40`), and hit **Generate**. It gives you the URL list and a printable sheet of codes + QR codes.
2. With the free **NFC Tools** app, for each tag: **Write → Add a record → URL/URI** → paste that tag's URL (`https://terra-garden-f8f03.web.app/?met=T07`) → **Write**, hold the phone on the sticker. Optional: **Other → Lock tag**. Label the tag with its code.
3. Hand them out. Each person **taps their own tag first**: the site opens with that code pre-filled ("Tapped your own bracelet? Its code T07 is filled in"), they add their name and plant themselves. That claims the tag.
4. From then on, anyone who taps that tag connects with its owner (both ways). Tapping your own tag again just opens your garden.

A bracelet is a tag plus LEDs: its tag holds the bracelet's code (`A1`), and the firmware carries the same code so the LEDs follow that person. Stick the tag away from the battery and strip (metal weakens NFC). Phones open the link automatically on tap — no app needed for guests.

**One bracelet, many tags** (the current hardware): the bracelet's tag is `A1`; everyone else gets a `T##` tag. Whoever the bracelet wearer meets — by tapping their tag, or them tapping the bracelet — lights one more LED on the wrist. Everyone else's flower grows on their phone.

---

## 7. Flash a bracelet

### One-time computer setup

1. **Arduino IDE 2.x** — <https://www.arduino.cc/en/software>.
2. **File → Preferences → Additional boards manager URLs**, add
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. **Tools → Board → Boards Manager** → search `esp32` → install **esp32 by Espressif Systems**.
4. **Tools → Manage Libraries** → install **Adafruit NeoPixel**. (That's the only library.)

### Per bracelet

1. Open `firmware/garden_bracelet/garden_bracelet.ino`. In the **SETTINGS** block at the top set `BRACELET_ID` (the code on its tag), `WIFI_SSID` / `WIFI_SSID_PREFIX` / `WIFI_PASSWORD`. Project id and API key are already filled in.
2. USB-C in. **Tools → Board → esp32 → XIAO_ESP32C3**, pick the **Port**.
3. **Upload**. If it can't connect: hold **B** (boot), tap **R** (reset), release **B**, upload again.
4. **Serial Monitor** at 115200: `Connecting to "…"` → `Connected` → `Bracelet not planted yet` (amber breathing) until its owner plants themselves with that code, then `Planted as fern`, `Connections: 0`.

### Wi-Fi at the event

The firmware has a short **network list** in its SETTINGS block, tried in order:

```cpp
const WifiNet NETWORKS[] = {
  { "USC Guest Wireless",           "USC Guest Wireless", ""                 },  // open campus Wi-Fi
  { "Alex\xe2\x80\x99s iPhone (3)", "Alex",              "2444666668888888" },  // backup: hotspot
};
```

On boot (and whenever Wi-Fi drops) the bracelet scans, joins the first listed network that's in range, and then checks it can actually reach the internet. If it can't — no signal upstream, or a sign-in / accept-terms page — it disconnects and tries the next one. So on campus it uses **USC Guest Wireless** (open, no password; USC ITS lists it as needing no login), and anywhere else it falls back to the hotspot. Add, remove or reorder lines to change that; use `""` as the password for an open network.

The ESP32 is 2.4 GHz only and can't click through a login page, so any network that needs one will be skipped. **iPhone hotspot:** Settings → Personal Hotspot → **Allow Others to Join** and **Maximize Compatibility** ON, and keep that screen open while the bracelet boots (iPhones stop advertising after ~90 s with no clients). The `prefix` column handles the curly apostrophe in iPhone hotspot names.

Serial Monitor shows the whole decision: `Scan: 14 networks` → `Connecting to "USC Guest Wireless" (open)… connected` → `internet check: HTTP 204` → `Online via "USC Guest Wireless"`. A code other than 204 means that network was skipped.

The bracelet only sends a few hundred bytes every 3 s, so a weak signal is fine.

### LED demo without Wi-Fi

`firmware/bloom_demo/bloom_demo.ino` plays the "meeting people" animation on a loop — petals bloom in one by one, sparkle at nine, wilt, repeat. Useful for checking wiring or filming. Colour and pacing are settings at the top.

---

## 8. What the bracelet's lights mean

| Pattern | Meaning |
|---|---|
| LED 1 breathing blue | Connecting to Wi-Fi |
| LED 1 breathing amber | Online, but nobody has planted themselves with this code yet |
| LEDs bloom in one by one, in a colour | Owner found; that colour is their plant; that's their connection count |
| One more LED fades in | Someone just connected with them |
| All 9 lit + white sparkle wave | Full bloom |
| LED 1 double-blinks red | Request to Firestore failed; retries every 3 s, reconnects Wi-Fi after 5 failures |

---

## 9. Demo-day flow

1. Everyone opens the site (tap a bracelet, scan someone's QR, or go to the URL) and plants themselves — bracelet wearers type their code, everyone else leaves it blank.
2. Meet people: tap a bracelet or scan a QR. Both flowers grow live; bracelets bloom one more LED within ~3 s.
3. Nine connections: Full Bloom on screen, white sparkle on the wrist.
4. **Remove a connection:** double-tap (or double-click) a person's flower icon under **People you've met**. The connection is removed for both people, both counts drop by one, and any bracelet loses an LED within ~3 s. They reappear under Demo mode so you can re-add them.
5. **Hide / Show QR** in the corner of the **Your link** card, when you don't want the code on screen. The choice is remembered on that phone.
6. **Demo mode** at the bottom lists everyone in the garden, for connecting without reaching a phone.
7. **Reset my garden** removes that guest everywhere and frees their code.

---

## 10. Testing locally (optional)

`firebase emulators:start` (needs Java) runs Firestore + hosting on your laptop. Open `http://localhost:5000/?emulator&met=A1` — the `?emulator` flag points the page at the local database. The firmware always talks to the real project.

---

## 11. Troubleshooting

| Symptom | Fix |
|---|---|
| Page: "Could not reach the garden" | Rules not published, or `firebase-config.js` keys wrong. Browser console (F12) shows `permission-denied` / `invalid-api-key`. |
| Page: "Bracelet X hasn't been planted yet" | That person hasn't signed up, or their tag was written with a different code than they typed. |
| Bracelet stays amber after its owner planted | `BRACELET_ID` in the firmware ≠ the code they typed. Check Firestore → Data → `guests`. |
| Red double-blink; Serial says `HTTP 403` | Rules not published / `allow read` broken. |
| Red double-blink; `HTTP 404` on a code you *know* is planted | `FIREBASE_PROJECT_ID` typo. |
| Red double-blink; `-1` | Wi-Fi has no internet (hotspot data off, captive portal). |
| Blue breathing never stops | No listed network in range, wrong password, hotspot asleep, or 5 GHz-only network. Serial Monitor says which. |
| Serial: `internet check: HTTP 302`/`200` on USC Guest Wireless | That access point is showing a sign-in page. The bracelet moves on to the hotspot automatically. |
| `fatal error: Adafruit_NeoPixel.h: No such file` | Install the library (Tools → Manage Libraries). |
| Colours wrong / flicker | Add the capacitor; try `NEO_RGB` instead of `NEO_GRB` in the `.ino`. |
| Someone claimed the wrong code | They tap **Reset my garden** and plant again. |
| Want stricter security (post-demo) | Add Firebase Anonymous Auth, store `owner: request.auth.uid` on the guest doc, restrict update/delete to the owner. Bracelet reads stay public. |
