# The Garden 🌸

*Terra Labs · Demo Day*

A wearable flower that blooms as you meet people. Every guest has a personal link (on an NFC bracelet, a QR code on their phone, or just a URL). Open someone's link and you've met: both flowers grow on screen, and if either of you is wearing a bracelet, one more LED lights up on their wrist. Nine connections is full bloom.

**Live:** <https://terra-garden-f8f03.web.app>

```
   open someone's link  →  The Garden web app  →  Firestore  ←  bracelet (ESP32) polls its count
   (NFC / QR / URL)         records A ↔ B            guests/A.count   and lights that many LEDs
```

## What's here

| Folder | Contents |
|---|---|
| `web/` | The site (plain HTML/CSS/JS + Firebase Firestore, no build step). |
| `firmware/garden_bracelet/` | XIAO ESP32-C3 firmware — one `.ino`, settings at the top. |
| `firmware/bloom_demo/` | LED-only demo animation, no Wi-Fi. |
| `firebase/` | Firestore security rules. |
| `SETUP_GUIDE.md` | Parts, wiring, Firebase, deploy, NFC tags, flashing, troubleshooting. |

## Quick start

**Use it:** tap your NFC tag (or open the live site), plant yourself, then tap other people's tags / show your QR.

**Make tags:** <https://terra-garden-f8f03.web.app/tags.html> generates a batch of codes with URLs + QR codes; write each URL to a tag with the NFC Tools app.

**Deploy the site** (after editing `web/`):

```bash
npx -y firebase-tools login
npx -y firebase-tools deploy --only hosting,firestore --project terra-garden-f8f03
```

**Flash a bracelet:** Arduino IDE → board `XIAO_ESP32C3` → library `Adafruit NeoPixel` → open `firmware/garden_bracelet/garden_bracelet.ino`, set `BRACELET_ID` and Wi-Fi in the SETTINGS block → Upload.

## Hardware (per bracelet)

Seeed XIAO ESP32-C3 · 9× WS2812B LEDs · 3.7 V LiPo · 330 Ω resistor · 470–1000 µF cap · NTAG213 NFC sticker. Battery feeds the XIAO (BAT pads) and the strip separately; one data wire from D0. No voltage converter needed. Full wiring in the guide.

## How the data works

- `guests/{CODE}` — `name`, `answer`, `plant`, `photo`, `count`
- `guests/{CODE}/contacts/{OTHER}` — `name`, `plant`, `metAt`

Opening `/?met=CODE` records the connection for **both** people in one transaction. The bracelet firmware does a plain HTTPS `GET` on its own `guests/{CODE}` document every 3 s (public read, no auth) and lights `count` LEDs in the owner's plant colour. Codes are claimed once; the only field that changes afterwards is `count`.

## Status

Live and tested end to end: site deployed, Firestore rules published, symmetric connections verified on the real project, firmware built for the one physical bracelet (`A1`). The demo-day trust model is open (no sign-in); see the guide's last row for the post-demo lockdown.
