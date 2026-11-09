#pragma once

#include <cstdint>

// UID -> HID keyboard mapping.
//
// Each entry ties one full NFCID1 (the tag UID) to the key that is held while
// the tag is present. Tags missing from this table produce no keyboard events
// at all; they only show up in the debug log.
//
// Up to six entries may be held at the same time (six keycode slots in the HID
// boot-keyboard report) while any number of modifier bits ride along. Entries
// sharing a modifier are handled correctly: Shift stays down until the last tag
// that needs it is removed.
//
// HID keycodes are the raw values from the USB HID Usage Tables, chapter 7,
// "Keyboard/Keypad Page (0x07)". Common values:
//
//   0x04..0x1D  A..Z             0x1E..0x27  1..9,0
//   0x28 Enter 0x29 Esc 0x2A BkSp 0x2B Tab 0x2C Space
//   0x3A F1 .. 0x45 F12          0x50..0x63  arrows, insert, etc.
//
// Modifiers use raw HID modifier bits:
//
//   0x01 Left Ctrl  0x02 Left Shift  0x04 Left Alt  0x08 Left GUI
//   0x10 Right Ctrl 0x20 Right Shift 0x40 Right Alt 0x80 Right GUI
//
// Entry format:
//
//   { {0x04,0xA1,0xB2,0xC3,0xD4,0xE5,0xF6}, 7, 0x05 /* B */, 0x02 /* Shift */ },
//
// `len` is the UID length in bytes (7 for the tags in this reader, 4 or 10 for
// other NFC-A tags). `modifier` may be 0 for a plain key.

namespace keymap {

constexpr uint8_t MAX_UID_LEN = 10;

struct Entry {
  uint8_t uid[MAX_UID_LEN];
  uint8_t len;
  uint8_t keycode;
  uint8_t modifier;
};

// Intentionally empty: fill in the panel's UIDs as they become known. An empty
// table makes the reader emit no keyboard events while still logging every tag.
constexpr Entry ENTRIES[] = {
    // { {0x04,0xA1,0xB2,0xC3,0xD4,0xE5,0xF6}, 7, 0x05, 0x02 },
};

constexpr uint8_t COUNT = sizeof(ENTRIES) / sizeof(ENTRIES[0]);

}  // namespace keymap
