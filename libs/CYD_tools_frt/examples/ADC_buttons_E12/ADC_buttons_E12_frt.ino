// by Marius Versteegen, 2026

// The ino code has been moved to a header file such that it
// can be inspected in non-Arduino IDE environments with
// proper code highlighting and intellisense too.
//
// De naam heeft _frt erachter, zodat hij niet botst met ADC_buttons_E12.ino van
// CYD_tools_crt: beide examples staan in de includepaden van main/CMakeLists.txt.

#include "ADC_buttons_E12_frt_ino.h"

// IMPORTANT: required for this example to work:
// if you are building from Arduino IDE, make sure to set
// the build setting "Core Debug Level" to "info"
// and select baudrate=115200 in Serial Monitor.
