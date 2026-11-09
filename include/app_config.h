#pragma once

#include <cstdint>

namespace app {

// ST25R300 SPI is specified up to 10 MHz.
constexpr uint32_t SPI_SPEED_HZ = 10000000;

// Maximum tags resolved in one antenna sweep. The RFAL NFCA collision resolver
// accepts up to 255, but 32 is ample for this panel and bounds the buffer.
constexpr uint8_t MAX_TAGS = 32;

// Maximum simultaneously tracked UIDs across both antennas. Must be at least
// MAX_TAGS so a single sweep cannot overflow the table.
constexpr uint8_t MAX_TRACKED_UIDS = 64;

// TLP3906 photovoltaic coupler: ton 0.2 ms typ, toff 0.3 ms typ, 1 ms max.
// The board drives the coupler LEDs at about 17 mA, so 1 ms is conservative.
constexpr uint32_t ANTENNA_SETTLE_US = 250;

// Number of complete A+B rounds a tracked UID must be missing before it is
// reported as removed. Two rounds absorb a single missed read without
// re-reporting the tag, and worst-case removal latency is about 240 ms.
// This also bounds when a held key is released in keyboard mode.
constexpr uint8_t ABSENT_ROUNDS_BEFORE_REMOVE = 2;

// Read and print the chip ID and per-antenna amplitude/phase at boot.
constexpr bool ENABLE_RF_DIAGNOSTICS = true;

// Master mode switch.
//
//  true  - debug: no keyboard device, and the boot wait blocks until the host
//          sends a character so the banner is not lost.
//  false - keyboard: the board enumerates as a USB HID keyboard and presses the
//          mapped key while a registered tag is present.
constexpr bool DEBUG_MODE = true;

// When true, setup() prints nothing until the host sends any character. This
// keeps the boot banner from being lost before a serial monitor attaches.
// While active, RF does not start until the host responds either. Only
// meaningful in debug mode; keyboard mode never blocks on serial input.
constexpr bool WAIT_FOR_SERIAL_INPUT = DEBUG_MODE;

// Wait up to this long for the USB serial host before starting RF.
constexpr uint32_t USB_WAIT_MS = 1500;

}  // namespace app
