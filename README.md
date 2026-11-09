# egasumi_fw

Firmware for the egasumi ST25R300 + Raspberry Pi Pico 2 NFC reader.

The reader continuously inventories ISO/IEC 14443 Type A / NFC Forum Type 2
tags on two antenna coils. It performs a full UID collision resolution on
antenna A, then on antenna B, and repeats. No tag memory is read or written.

Output is an event log rather than a per-sweep dump: one line when a UID first
appears, one line when it stops being readable on either coil.

## Hardware

| Signal | Pico 2 GPIO |
| --- | --- |
| SPI0 SCK | 18 |
| SPI0 MOSI | 19 |
| SPI0 MISO | 16 |
| ST25R300 CS | 17 |
| ST25R300 IRQ | 20 |
| Antenna A select | 15 |
| Antenna B select | 14 |

The ST25R300 RESET pin is tied to GND on the board, so the firmware does not
drive a reset GPIO. The RF stage is configured for differential antenna drive
(`GENERAL.single = 0`, RFO1/RFO2). Exactly one antenna select GPIO is HIGH at
a time; both are updated atomically in one register write.

## Build

The RFAL core and ST25R300/ST25R500 driver are Git submodules:

- `lib/NFC-RFAL` (stm32duino/NFC-RFAL 2.0.2)
- `lib/ST25R500` (stm32duino/ST25R500 1.0.0)

```sh
git submodule update --init --recursive
pio run
```

The build uses the earlephilhower Arduino core for the RP2350, TinyUSB CDC,
SPI mode 1 at 10 MHz, and `RFAL_CMATH`. Listen mode is disabled through
`include/rfal_custom_config.h`; this revision of the ST25R500 glue references
listen-mode helpers that are not present in NFC-RFAL 2.0.2.

The resulting image is `.pio/build/pico2/firmware.uf2`.

## Modes

`DEBUG_MODE` in `include/app_config.h` selects the behavior at compile time.

| | `DEBUG_MODE = true` | `DEBUG_MODE = false` |
| --- | --- | --- |
| USB device | CDC serial only | CDC serial **and** HID keyboard |
| Key events | never emitted | up to 6 mapped tags each hold their own key |
| Boot | waits for a character (banner preserved) | starts immediately |

The keyboard device is absent from the descriptor in debug mode, so a debug
build can never type into the host machine.

### Keyboard mode

A mapped UID presses its key as soon as an antenna sweep finds it, before the
other coil is read, so detection latency is one sweep plus one HID report. The
key stays held while the tag remains readable and is released once the tag has
been absent for `ABSENT_ROUNDS_BEFORE_REMOVE` complete rounds.

Pressing happens through the Arduino `Keyboard` / `HID_Keyboard` libraries, which
drive the standard HID boot-keyboard report: six keycode slots plus one modifier
byte. Up to six mapped tags can therefore be held at once, each with its own
key and modifiers, and modifier bits are shared correctly — a `Shift` needed by
two held tags stays down until the last of them is released. A seventh tag is
logged but gets no key until a slot frees up.

Unregistered tags never produce key events. They appear in the log only.

The USB stack is the core's native device stack (not Adafruit TinyUSB), because
the Arduino `Keyboard` library is incompatible with TinyUSB; the native stack
composes CDC and HID into one composite device. See `platformio.ini`.

## Serial output

USB CDC, in both modes. Lines are emitted only when presence changes:

```
[+] 04A1B2C3D4E5F6 AB key='b' (0x62)
[+] 04B1C2D3E4F5A6 A
[-] 04A1B2C3D4E5F6 key='b' (0x62)
```

- `[+] <UID> <A|B|AB>`: the UID was not previously readable; the label says
  which coil(s) answered during the round in which it appeared.
  `AB` means both coils detected the tag.
- `key=...`: present for tags found in `include/keymap.h`. Printable characters
  are quoted; non-printing keys show the raw library value, so `KEY_CAPS_LOCK`
  appears as `key=0xC1`. Unregistered tags print the bare UID instead.
- `[-] <UID>`: the UID was no longer readable on either coil.

UIDs are uppercase hex, all bytes of NFCID1 (14 digits for the 7-byte tags in
use).

In keyboard mode the log is suppressed while no serial host is connected, so a
standalone keyboard never stalls on a full USB transmit buffer.

The label also re-reports a tag when its coil set changes, for example
`AB` -> `A` as the tag moves out of coil B's field, or `A` -> `B` when it
moves to the other coil. A tag that stays readable on the same coil set
produces no output.

`err <rc>` is printed for non-recoverable sweep errors (SPI, wrong state).

### Hysteresis

A UID must be missing for `ABSENT_ROUNDS_BEFORE_REMOVE` complete A+B rounds
(default 2) before `[-]` is emitted. A single missed read therefore does not
produce a spurious `[-]` / `[+]` pair when the tag is still present.

## UID to key mapping

The map lives in `include/keymap.h` and is intentionally empty. Add entries as
the panel's tag UIDs become known, writing keys the same way as ordinary Arduino
keyboard code:

```cpp
constexpr Entry ENTRIES[] = {
    { {0x04,0xA1,0xB2,0xC3,0xD4,0xE5,0xF6}, 7, 'b' },
    { {0x04,0xB1,0xC2,0xD3,0xE4,0xF5,0xA6}, 7, KEY_CAPS_LOCK },
};
```

Fields are `uid`, `len`, `key`. Use lowercase characters for letters and digits
(no Shift needed, one keycode slot per tag), and `KEY_*` macros from
`HID_Keyboard.h` for keys that have no character, such as `KEY_CAPS_LOCK`,
`KEY_F1`, `KEY_UP_ARROW` or `KEY_DELETE`. Modifier macros such as
`KEY_LEFT_SHIFT` can be given to their own tag and are held while it is
present; several tags sharing one modifier release it only when the last of them
is removed. There are no `KEY_A`..`KEY_Z` macros, so letters must be characters.
Every UID must be listed in full; there is no prefix matching.

## Boot output

At boot the firmware prints the ST25R300 IC ID, the differential antenna mode
result, the NFCA poller initialization result, and a rough amplitude/phase
measurement for each antenna.

With `WAIT_FOR_SERIAL_INPUT` enabled (the default, tied to `DEBUG_MODE`),
nothing is printed and RF does not start until the host sends any character.
Open the serial monitor and press Enter once; the banner then appears in full
instead of being lost to a port that was not attached yet.

## Timing

- SPI: 10 MHz (ST25R300 maximum).
- Antenna switch settle: `ANTENNA_SETTLE_US`, currently 250 us.
- NFC-A guard time after field-on: RFAL `RFAL_GT_NFCA`, 5 ms, waited once per
  field-on cycle. The RF field is off while the antenna path changes.
- Maximum tags per sweep: 32.
- Key press latency: one sweep (field on, 5 ms GT, collision resolution) plus
  one 1 ms HID poll interval, measured from the sweep that first sees the tag.
- Key release latency: up to 2 complete A+B rounds.

## Status

The firmware builds cleanly. It has not been run on hardware; the board was not
connected during development.
