// ============================================================
//  Paste your Firebase web-app config here.
//  Firebase console → Project settings → Your apps → Web app → "Config"
//
//  These values are safe to ship in a public page; access is controlled
//  by the Firestore security rules in firebase/firestore.rules.
// ============================================================
export const firebaseConfig = {
  apiKey:            "YOUR-WEB-API-KEY",
  authDomain:        "YOUR-PROJECT-ID.firebaseapp.com",
  projectId:         "YOUR-PROJECT-ID",
  storageBucket:     "YOUR-PROJECT-ID.appspot.com",
  messagingSenderId: "000000000000",
  appId:             "1:000000000000:web:0000000000000000000000",
};

// How many connections make a full bloom.
// MUST equal NUM_LEDS in firmware/garden_bracelet/config.h  (9 LEDs on the strip).
export const BLOOM_AT = 9;
