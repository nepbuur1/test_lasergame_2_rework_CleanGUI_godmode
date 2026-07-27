// The Image objects of the Panel example - by Marius Versteegen, 2026
//
// Every image has its own header holding just the pixel array; this file
// gathers them and declares one Image per array. Everything an image needs
// beyond its pixels - its size, and which colour acts as transparent - is
// passed to the constructor here, so the pixel headers stay free of it.
//
// Include this file, not the individual pixel headers.

#pragma once
#include <crt_Image.h>

#include "gradient.h"
#include "ring.h"

namespace crt
{
	// Opaque image: every pixel is written.
	constexpr Image imageGradient(arGradientPixels, 64, 48);

	// A target ring in which magenta (ARGB 0x00FF00FF) acts as transparent:
	// those pixels are skipped, so whatever the parent drew underneath stays
	// visible.
	constexpr Image imageRing(arRingPixels, 64, 64, 0x00FF00FF);
};
