#include <Arduino.h>
#include <SPI.h>
#include <hardware/gpio.h>

#include "app_config.h"
#include "board_pins.h"
#include "rfal_nfc.h"
#include "rfal_rfst25r500.h"
#include "st_errno.h"

namespace {

RfalRfST25R500Class rfReader(&SPI, board::PIN_SPI_CS, board::PIN_IRQ,
                             board::PIN_RF_RESET, app::SPI_SPEED_HZ);
RfalNfcClass nfc(&rfReader);

rfalNfcaListenDevice gTags[app::MAX_TAGS];

// Worst case: antenna letter + (space + 10-byte UID) per tag + CRLF.
constexpr size_t kLineSize =
    1 + app::MAX_TAGS * (1 + 2 * RFAL_NFCA_CASCADE_3_UID_LEN) + 2;
char gLine[kLineSize];

constexpr char kHex[] = "0123456789ABCDEF";

constexpr uint32_t kAntennaMask =
    (1UL << board::PIN_ANT_A) | (1UL << board::PIN_ANT_B);
constexpr uint32_t kAntennaA = 1UL << board::PIN_ANT_A;
constexpr uint32_t kAntennaB = 1UL << board::PIN_ANT_B;

void selectAntenna(bool antennaA) {
  // The RF field must be off here. Update both gate pins in one register write
  // so there is no interval where both are LOW or both are HIGH.
  gpio_put_masked(kAntennaMask, antennaA ? kAntennaA : kAntennaB);
  delayMicroseconds(app::ANTENNA_SETTLE_US);
}

bool isNoTagOrRecoverable(ReturnCode err) {
  switch (err) {
    case ERR_NONE:
    case ERR_REQUEST:
    case ERR_TIMEOUT:
    case ERR_NOTFOUND:
    case ERR_NOTUNIQUE:
    case ERR_RF_COLLISION:
    case ERR_CRC:
    case ERR_FRAMING:
    case ERR_PAR:
    case ERR_INCOMPLETE_BYTE:
      return true;
    default:
      return false;
  }
}

void emitSweep(bool antennaA, uint8_t count, ReturnCode err) {
  if (!Serial) {
    return;
  }

  const char antenna = antennaA ? 'A' : 'B';

  if (count == 0) {
    if (isNoTagOrRecoverable(err)) {
      Serial.printf("%c -\r\n", antenna);
    } else {
      Serial.printf("%c !%d\r\n", antenna, static_cast<int>(err));
    }
    return;
  }

  char *p = gLine;
  *p++ = antenna;

  for (uint8_t i = 0; i < count; ++i) {
    *p++ = ' ';
    uint8_t len = gTags[i].nfcId1Len;
    if (len > RFAL_NFCA_CASCADE_3_UID_LEN) {
      len = RFAL_NFCA_CASCADE_3_UID_LEN;
    }
    for (uint8_t b = 0; b < len; ++b) {
      const uint8_t value = gTags[i].nfcId1[b];
      *p++ = kHex[value >> 4];
      *p++ = kHex[value & 0x0FU];
    }
  }

  *p++ = '\r';
  *p++ = '\n';
  Serial.write(gLine, static_cast<size_t>(p - gLine));
}

void measureAntenna(bool antennaA) {
  selectAntenna(antennaA);

  ReturnCode err = rfReader.rfalFieldOnAndStartGT();
  if (err != ERR_NONE) {
    Serial.printf("diag %c: field on failed (%d)\r\n", antennaA ? 'A' : 'B',
                  static_cast<int>(err));
    rfReader.rfalFieldOff();
    return;
  }

  while (!rfReader.rfalIsGTExpired()) {
    yield();
  }

  uint8_t amplitude = 0;
  uint8_t phase = 0;
  const ReturnCode ampErr = rfReader.rfalChipMeasureAmplitude(&amplitude);
  const ReturnCode phaseErr = rfReader.rfalChipMeasurePhase(&phase);

  rfReader.rfalFieldOff();

  Serial.printf("diag %c: amp=%u (%d) phase=%u (%d)\r\n",
                antennaA ? 'A' : 'B', static_cast<unsigned>(amplitude),
                static_cast<int>(ampErr), static_cast<unsigned>(phase),
                static_cast<int>(phaseErr));
}

bool initializeReader() {
  for (uint8_t attempt = 0; attempt < 5; ++attempt) {
    const ReturnCode err = nfc.rfalNfcInitialize();
    if (err == ERR_NONE) {
      return true;
    }
    Serial.printf("RFAL init failed (%d), retrying\r\n",
                  static_cast<int>(err));
    delay(500);
  }
  return false;
}

}  // namespace

void setup() {
  // Select antenna A before the RF stage can transmit. Set the output latch
  // first so enabling the pin does not produce a low pulse on A.
  gpio_put_masked(kAntennaMask, kAntennaA);
  pinMode(board::PIN_ANT_A, OUTPUT);
  pinMode(board::PIN_ANT_B, OUTPUT);
  delayMicroseconds(app::ANTENNA_SETTLE_US);

  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && (millis() - waitStart) < app::USB_WAIT_MS) {
    delay(10);
  }

  Serial.println();
  Serial.println("EGASUMI NFC READER");
  Serial.println("ST25R300, NFCA 106 kbps, differential antenna, SPI 10 MHz");

  SPI.begin();
  pinMode(board::PIN_SPI_CS, OUTPUT);
  digitalWrite(board::PIN_SPI_CS, HIGH);

  if (!initializeReader()) {
    Serial.println("RFAL initialization failed; halting");
    for (;;) {
      delay(1000);
    }
  }

  uint8_t icId = 0;
  const ReturnCode idErr = rfReader.st25r500ReadRegister(ST25R500_REG_IC_ID,
                                                         &icId);
  Serial.printf("IC_ID=0x%02X type=%u rev=%u (%d)\r\n",
                static_cast<unsigned>(icId),
                static_cast<unsigned>((icId >> 3) & 0x1FU),
                static_cast<unsigned>(icId & 0x07U),
                static_cast<int>(idErr));

  const ReturnCode modeErr = rfReader.rfalChipSetAntennaMode(false, false);
  Serial.printf("differential antenna mode: %d\r\n",
                static_cast<int>(modeErr));

  const ReturnCode nfcaErr = nfc.rfalNfcaPollerInitialize();
  Serial.printf("NFCA poller init: %d\r\n", static_cast<int>(nfcaErr));
  if (nfcaErr != ERR_NONE) {
    Serial.println("NFCA poller initialization failed; halting");
    for (;;) {
      delay(1000);
    }
  }

  if (app::ENABLE_RF_DIAGNOSTICS) {
    measureAntenna(true);
    measureAntenna(false);
    selectAntenna(true);
  }

  Serial.println("ready");
}

void loop() {
  for (uint8_t i = 0; i < 2; ++i) {
    const bool antennaA = (i == 0);

    // The field is turned off at the end of every sweep, so the antenna path
    // can be changed here without another field-off transaction.
    selectAntenna(antennaA);

    ReturnCode err = rfReader.rfalFieldOnAndStartGT();
    if (err != ERR_NONE) {
      rfReader.rfalFieldOff();
      emitSweep(antennaA, 0, err);
      continue;
    }

    while (!rfReader.rfalIsGTExpired()) {
      yield();
    }

    uint8_t count = 0;
    err = nfc.rfalNfcaPollerFullCollisionResolution(
        RFAL_COMPLIANCE_MODE_NFC, app::MAX_TAGS, gTags, &count);

    rfReader.rfalFieldOff();
    emitSweep(antennaA, count, err);
  }
}
