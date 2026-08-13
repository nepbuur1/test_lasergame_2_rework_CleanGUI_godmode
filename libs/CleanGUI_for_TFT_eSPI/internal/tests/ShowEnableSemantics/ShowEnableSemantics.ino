// by Marius Versteegen, 2026

// The ino code has been moved to a header file such that it
// can be inspected in non-Arduino IDE environments with
// proper code highlighting and intellisense too.

#include "ShowEnableSemantics_ino.h"

// Deze test vraagt geen enkele aanraking: hij maakt ze zelf na. Draaien en de
// log lezen is genoeg - de laatste regel is het oordeel.
//
// Staat in internal/tests en niet bij de voorbeelden: een voorbeeld is er om
// van te leren, deze om te falen zodra show, enable of tekenen weer in elkaar
// gaan grijpen.

// IMPORTANT: required for this example to work:
// if you are building from Arduino IDE, make sure to set
// the build setting "Core Debug Level" to "info"
// and select baudrate=115200 in Serial Monitor.
