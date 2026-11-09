#pragma once

// Start from the ST defaults, then trim the RFAL features this reader does not
// use. The ST25R500 RFAL glue in this library revision references listen-mode
// helpers that the NFC-RFAL 2.0.2 core does not provide, so listen mode must be
// disabled for the library to compile.
#include "rfal_default_config.h"

#undef RFAL_FEATURE_LISTEN_MODE
#define RFAL_FEATURE_LISTEN_MODE false
