#pragma once

#include <cstdint>

namespace app {

// ST25R300 SPI is specified up to 10 MHz.
constexpr uint32_t SPI_SPEED_HZ = 10000000;

// Maximum tags resolved in one antenna sweep. The RFAL NFCA collision resolver
// accepts up to 255, but 32 is ample for this panel and bounds the buffer.
constexpr uint8_t MAX_TAGS = 32;

// TLP3906 photovoltaic coupler: ton 0.2 ms typ, toff 0.3 ms typ, 1 ms max.
// The board drives the coupler LEDs at about 17 mA, so 1 ms is conservative.
constexpr uint32_t ANTENNA_SETTLE_US = 1000;

// Read and print the chip ID and per-antenna amplitude/phase at boot.
constexpr bool ENABLE_RF_DIAGNOSTICS = true;

// Wait up to this long for the USB serial host before starting RF.
constexpr uint32_t USB_WAIT_MS = 1500;

}  // namespace app
