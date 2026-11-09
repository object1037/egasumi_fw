#pragma once

#include <cstdint>

namespace board {

// SPI0 on the Pico 2.
constexpr uint8_t PIN_SPI_SCK  = 18;
constexpr uint8_t PIN_SPI_MOSI = 19;
constexpr uint8_t PIN_SPI_MISO = 16;
constexpr uint8_t PIN_SPI_CS   = 17;

// ST25R300 interrupt output.
constexpr uint8_t PIN_IRQ = 20;

// Antenna selection. Exactly one of these is HIGH during normal operation.
// HIGH on PIN_ANT_A enables coil A, HIGH on PIN_ANT_B enables coil B.
constexpr uint8_t PIN_ANT_A = 15;
constexpr uint8_t PIN_ANT_B = 14;

// The board ties ST25R300 RESET (pin 29) to GND. RESET is active-high on the
// ST25R300, so no MCU reset GPIO is available.
constexpr int PIN_RF_RESET = -1;

}  // namespace board
