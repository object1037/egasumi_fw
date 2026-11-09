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

## Serial output

USB CDC. Lines are emitted only when presence changes:

```
[+] 04A1B2C3D4E5F6 AB
[+] 04B1C2D3E4F5A6 A
[-] 04A1B2C3D4E5F6
```

- `[+] <UID> <A|B|AB>`: the UID was not previously readable; the label says
  which coil(s) answered during the round in which it appeared.
  `AB` means both coils detected the tag.
- `[-] <UID>`: the UID was no longer readable on either coil.
- UID is uppercase hex, all bytes of NFCID1 (14 digits for the 7-byte tags in
  use).

The label also re-reports a tag when its coil set changes, for example
`AB` -> `A` as the tag moves out of coil B's field, or `A` -> `B` when it
moves to the other coil. A tag that stays readable on the same coil set
produces no output.

`err <rc>` is printed for non-recoverable sweep errors (SPI, wrong state).

### Hysteresis

A UID must be missing for `ABSENT_ROUNDS_BEFORE_REMOVE` complete A+B rounds
(default 2) before `[-]` is emitted. A single missed read therefore does not
produce a spurious `[-]` / `[+]` pair when the tag is still present.

## Boot output

At boot the firmware prints the ST25R300 IC ID, the differential antenna mode
result, the NFCA poller initialization result, and a rough amplitude/phase
measurement for each antenna.

## Timing

- SPI: 10 MHz (ST25R300 maximum).
- Antenna switch settle: 1 ms (TLP3906 worst-case `toff`).
- NFC-A guard time after field-on: RFAL `RFAL_GT_NFCA`, 5 ms, waited once per
  field-on cycle. The RF field is off while the antenna path changes.
- Maximum tags per sweep: 32.
- Removal latency: up to 2 complete A+B rounds, about 240 ms with a few tags.

## Status

The firmware builds cleanly. It has not been run on hardware; the board was not
connected during development.
