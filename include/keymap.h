#pragma once

#include <cstdint>

#include <HID_Keyboard.h>

// UID -> keyboard mapping.
//
// Each entry ties one full NFCID1 (the tag UID) to the key that is held while
// the tag is present. Tags missing from this table produce no keyboard events
// at all; they only show up in the debug log.
//
// Up to six entries may be held at the same time (six keycode slots in the HID
// boot-keyboard report), and any number of modifier tags may ride along.
//
// `key` is passed to Keyboard.press() unchanged, exactly like ordinary Arduino
// keyboard code, so both forms work:
//
//   'b'              a character; the library looks it up in the keyboard layout
//   KEY_CAPS_LOCK    a non-printing key macro from HID_Keyboard.h
//   KEY_LEFT_SHIFT   a modifier macro, held while the tag is present
//
// Use lowercase characters for letters and digits. Lowercase needs no Shift,
// which keeps each tag to exactly one key and avoids the library silently
// adding a modifier. Uppercase ('B') and shifted symbols ('!', '@') would work
// too, but they consume the Shift modifier for that tag.
//
// There are no KEY_A..KEY_Z macros; letters must be written as characters.
//
// Entry format:
//
//   { {0x04,0xA1,0xB2,0xC3,0xD4,0xE5,0xF6}, 7, 'b' },
//   { {0x04,0xB1,0xC2,0xD3,0xE4,0xF5,0xA6}, 7, KEY_CAPS_LOCK },
//
// `len` is the UID length in bytes (7 for the tags in this reader, 4 or 10 for
// other NFC-A tags).

namespace keymap {

constexpr uint8_t MAX_UID_LEN = 10;

struct Entry {
  uint8_t uid[MAX_UID_LEN];
  uint8_t len;
  uint8_t key;
};

// Intentionally empty: fill in the panel's UIDs as they become known. An empty
// table makes the reader emit no keyboard events while still logging every tag.
//
// The easiest way to fill it in: build with MODE = Mode::Capture, touch the tags
// in order (a..z then 0..9), send 'd' over serial, and paste the printed ENTRIES
// block over the one below. See README.md.
constexpr Entry ENTRIES[] = {
    // { {0x04,0xA1,0xB2,0xC3,0xD4,0xE5,0xF6}, 7, 'b' },
    // { {0x04,0xB1,0xC2,0xD3,0xE4,0xF5,0xA6}, 7, KEY_CAPS_LOCK },
};

constexpr uint8_t COUNT = sizeof(ENTRIES) / sizeof(ENTRIES[0]);

}  // namespace keymap
