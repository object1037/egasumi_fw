#include <Arduino.h>
#include <SPI.h>
#include <cstring>
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

// Tags found by the most recent sweep of one antenna. Only this array is
// handed to RFAL; presence tracking lives in gTable.
rfalNfcaListenDevice gTags[app::MAX_TAGS];

constexpr uint8_t kMaxUidLen = RFAL_NFCA_CASCADE_3_UID_LEN;

struct TrackedUid {
  bool used;
  bool seenA;           // present on coil A in the round being processed
  bool seenB;           // present on coil B in the round being processed
  bool reported;        // at least one [+] line has been emitted
  bool lastReportedA;   // coil A presence carried by the last [+] line
  bool lastReportedB;   // coil B presence carried by the last [+] line
  uint8_t len;
  uint8_t absentRounds;
  uint8_t uid[kMaxUidLen];
};

static_assert(app::MAX_TRACKED_UIDS >= app::MAX_TAGS,
              "tracking table must hold at least one full sweep");

TrackedUid gTable[app::MAX_TRACKED_UIDS];

// Presence accumulated by the current A+B round. Each tag is stored as a
// length byte followed by that many UID bytes, indexed by count.
constexpr uint16_t kSweepBytes = (app::MAX_TAGS * (kMaxUidLen + 1)) + 1;
struct SweepResult {
  uint8_t count;
  uint8_t tags[kSweepBytes];
};

SweepResult gSweepA;
SweepResult gSweepB;

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

void appendUid(SweepResult &sweep, const uint8_t *uid, uint8_t len) {
  if (sweep.count >= app::MAX_TAGS) {
    return;
  }
  if (len == 0U || len > kMaxUidLen) {
    return;
  }
  uint8_t *p = &sweep.tags[sweep.count * (kMaxUidLen + 1)];
  *p++ = len;
  memcpy(p, uid, len);
  ++sweep.count;
}

// Locate a UID in the tracking table, or return MAX_TRACKED_UIDS.
uint8_t findEntry(const uint8_t *uid, uint8_t len) {
  for (uint8_t i = 0; i < app::MAX_TRACKED_UIDS; ++i) {
    if (gTable[i].used && gTable[i].len == len &&
        memcmp(gTable[i].uid, uid, len) == 0) {
      return i;
    }
  }
  return app::MAX_TRACKED_UIDS;
}

// Returns the new entry index, or MAX_TRACKED_UIDS when the table is full.
uint8_t allocEntry() {
  for (uint8_t i = 0; i < app::MAX_TRACKED_UIDS; ++i) {
    if (!gTable[i].used) {
      return i;
    }
  }
  return app::MAX_TRACKED_UIDS;
}

void printUid(const uint8_t *uid, uint8_t len) {
  for (uint8_t b = 0; b < len; ++b) {
    Serial.write(kHex[uid[b] >> 4]);
    Serial.write(kHex[uid[b] & 0x0FU]);
  }
}

void reportAdded(const TrackedUid &entry) {
  Serial.print("[+] ");
  printUid(entry.uid, entry.len);
  Serial.print(' ');
  if (entry.seenA && entry.seenB) {
    Serial.print("AB");
  } else {
    Serial.print(entry.seenA ? 'A' : 'B');
  }
  Serial.print("\r\n");
}

void reportRemoved(const TrackedUid &entry) {
  Serial.print("[-] ");
  printUid(entry.uid, entry.len);
  Serial.print("\r\n");
}

// Emit a line for every non-recoverable sweep error. Recovery errors just
// mean the antenna saw no answer.
void reportSweepError(ReturnCode err) {
  if (err != ERR_NONE && !isNoTagOrRecoverable(err)) {
    Serial.printf("err %d\r\n", static_cast<int>(err));
  }
}

void readAntenna(bool antennaA, SweepResult &sweep) {
  sweep.count = 0;

  // The field is turned off at the end of every sweep, so the antenna path can
  // be changed here without another field-off transaction.
  selectAntenna(antennaA);

  ReturnCode err = rfReader.rfalFieldOnAndStartGT();
  if (err != ERR_NONE) {
    rfReader.rfalFieldOff();
    reportSweepError(err);
    return;
  }

  while (!rfReader.rfalIsGTExpired()) {
    yield();
  }

  uint8_t count = 0;
  err = nfc.rfalNfcaPollerFullCollisionResolution(
      RFAL_COMPLIANCE_MODE_NFC, app::MAX_TAGS, gTags, &count);

  rfReader.rfalFieldOff();

  if (count > 0U) {
    for (uint8_t i = 0; i < count; ++i) {
      uint8_t len = gTags[i].nfcId1Len;
      if (len > kMaxUidLen) {
        len = kMaxUidLen;
      }
      appendUid(sweep, gTags[i].nfcId1, len);
    }
    return;
  }

  reportSweepError(err);
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

// Reconcile the completed A+B round against the tracking table.
//
// Both sweeps are known before any line is emitted, so a tag present on both
// coils is reported once as "AB" instead of first as "A" and then again.
void updatePresence(const SweepResult &sweepA, const SweepResult &sweepB) {
  if (!Serial) {
    return;
  }

  // 1. Mark everything seen on either antenna and age out everything missing.
  //    New UIDs are recorded but stay unreported until their label is known.
  //    Clear last round's flags first: absent entries are identified by "!seenA
  //    && !seenB", so stale true bits would keep a removed tag alive forever.
  for (uint8_t i = 0; i < app::MAX_TRACKED_UIDS; ++i) {
    gTable[i].seenA = false;
    gTable[i].seenB = false;
  }

  for (const SweepResult *sweep : {&sweepA, &sweepB}) {
    const bool onA = (sweep == &sweepA);

    for (uint8_t i = 0; i < sweep->count; ++i) {
      const uint8_t *p = &sweep->tags[i * (kMaxUidLen + 1)];
      const uint8_t len = *p;

      uint8_t idx = findEntry(p + 1, len);
      if (idx == app::MAX_TRACKED_UIDS) {
        idx = allocEntry();
        if (idx == app::MAX_TRACKED_UIDS) {
          continue;  // Table full; skip rather than corrupt tracking state.
        }
        TrackedUid &fresh = gTable[idx];
        fresh = TrackedUid{};
        fresh.used = true;
        fresh.len = len;
        memcpy(fresh.uid, p + 1, len);
        fresh.reported = false;
        fresh.absentRounds = 0;
      }

      TrackedUid &entry = gTable[idx];
      if (onA) {
        entry.seenA = true;
      } else {
        entry.seenB = true;
      }
      entry.absentRounds = 0;
    }
  }

  // 2. Report additions and coil changes, then drop entries missing too long.
  for (uint8_t i = 0; i < app::MAX_TRACKED_UIDS; ++i) {
    TrackedUid &entry = gTable[i];
    if (!entry.used) {
      continue;
    }

    const bool seenNow = entry.seenA || entry.seenB;

    if (!seenNow) {
      if (entry.absentRounds < UINT8_MAX) {
        ++entry.absentRounds;
      }
      if (entry.absentRounds >= app::ABSENT_ROUNDS_BEFORE_REMOVE) {
        if (entry.reported) {
          reportRemoved(entry);
        }
        entry.used = false;
      }
      continue;
    }

    if (!entry.reported || entry.lastReportedA != entry.seenA ||
        entry.lastReportedB != entry.seenB) {
      // First sighting, or the tag appeared/disappeared on a coil.
      reportAdded(entry);
      entry.reported = true;
      entry.lastReportedA = entry.seenA;
      entry.lastReportedB = entry.seenB;
    }

    entry.seenA = false;
    entry.seenB = false;
  }
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

  if (app::WAIT_FOR_SERIAL_INPUT) {
    // Block before the banner so the host sees it from the first line. Waiting
    // for input rather than merely for connection also guards against a port
    // that opens and closes again while the board is enumerating.
    // Adafruit_USBD_CDC::available() yields internally, which services USB, so
    // this loop does not stall enumeration.
    while (!Serial.available()) {
    }
    while (Serial.available()) {
      Serial.read();  // Discard the wake-up bytes so the log starts clean.
    }
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
  readAntenna(true, gSweepA);
  readAntenna(false, gSweepB);
  updatePresence(gSweepA, gSweepB);
}
