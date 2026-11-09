#pragma once

// HID_Keyboard.h includes <tusb-hid.h>, expecting it to bring in the TinyUSB
// device API. The earlephilhower core ships that name as a header-only stub
// (libraries/tusb-hid/src/tusb-hid.h, contents: "// dummy") meant to pull in
// the TinyUSB HID class sources via its library.properties, not to provide any
// declarations.
//
// Under PlatformIO, HID_Keyboard is compiled without the tusb-hid include root
// because neither library declares the dependency, so the include fails. This
// local header, which lives on every build's include path via -I include,
// stands in for it and provides the real API HID_Keyboard and our firmware use.
#include <tusb.h>
