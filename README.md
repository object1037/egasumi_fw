# egasumi_fw

Firmware for the egasumi ST25R300 + Raspberry Pi Pico 2 NFC reader.

The reader continuously inventories ISO/IEC 14443 Type A / NFC Forum Type 2
tags on two antenna coils. It performs a full UID collision resolution on
antenna A, then on antenna B, and repeats. No tag memory is read or written.

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

USB CDC, one line per antenna sweep:

```
A 04A1B2C3D4E5F6 04B1C2D3E4F5A6
B -
A !4
```

- `A` / `B`: antenna that was swept.
- UID: uppercase hex, one token per tag, all bytes of NFCID1
  (7 bytes for the tags in use).
- `-`: no tag found.
- `!<rc>`: non-recoverable RFAL return code.

At boot the firmware prints the ST25R300 IC ID, the differential antenna mode
result, the NFCA poller initialization result, and a rough amplitude/phase
measurement for each antenna.

## Timing

- SPI: 10 MHz (ST25R300 maximum).
- Antenna switch settle: 1 ms (TLP3906 worst-case `toff`).
- NFC-A guard time after field-on: RFAL `RFAL_GT_NFCA`, 5 ms, waited once per
  field-on cycle. The RF field is off while the antenna path changes.
- Maximum tags per sweep: 32.

## Status

The firmware builds cleanly. It has not been run on hardware; the board was not
connected during development.
