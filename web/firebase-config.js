// ============================================================
//  Paste your Firebase web-app config here.
//  Firebase console → Project settings → Your apps → Web app → "Config"
//
//  These values are safe to ship in a public page; access is controlled
//  by the Firestore security rules in firebase/firestore.rules.
// ============================================================
export const firebaseConfig = {
  apiKey:            "AIzaSyDDr518HFVbY1-uyTdHRyiw7eh43KOxdAk",
  authDomain:        "terra-garden-f8f03.firebaseapp.com",
  projectId:         "terra-garden-f8f03",
  storageBucket:     "terra-garden-f8f03.firebasestorage.app",
  messagingSenderId: "1008857614568",
  appId:             "1:1008857614568:web:dc387d20ece1e8ee04215d",
};

// How many connections make a full bloom.
// MUST equal NUM_LEDS in firmware/garden_bracelet/garden_bracelet.ino (9 LEDs on the strip).
export const BLOOM_AT = 9;
