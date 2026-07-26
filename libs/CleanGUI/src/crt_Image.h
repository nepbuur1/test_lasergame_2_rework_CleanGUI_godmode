// by Marius Versteegen, 2026

#pragma once
#include <stdint.h>
#include "crt_typesAndConstants.h"

namespace crt
{
	// The pixel layout of the raw image data.
	// CleanGUI itself never looks at the pixels - it only passes this tag along,
	// so that a DisplayAdapter can either blit the data directly, convert it,
	// or refuse it.
	enum class PixelFormat : uint8_t
	{
		RGB565 = 0		// 16 bpp, one uint16_t per pixel, in host byte order.
	};

	// An Image is an opaque handle to pixel data that is owned elsewhere -
	// typically a "const" array in flash (.rodata). The Image object itself
	// holds no pixels and copies none: a DisplayAdapter blits straight from
	// the memory pData points at.
	//
	// Lifetime: the pixel data must outlive every draw call that refers to it.
	// For const arrays in flash that is true by definition. (Same agreement as
	// for the strings passed to drawString.)
	//
	// Transparency: colTransparent is in ARGB, like every other colour in
	// CleanGUI. The DisplayAdapter converts it to the image's pixel format and
	// compares; pixels that match are left untouched, so whatever the parent
	// drew underneath remains visible.
	class Image
	{
	public:
		const void* pData;
		int32_t     width;
		int32_t     height;
		PixelFormat format;
		bool        bHasTransparency;
		uint32_t    colTransparent;		// ARGB

	public:
		constexpr Image()
			: pData(nullptr), width(0), height(0), format(PixelFormat::RGB565),
			  bHasTransparency(false), colTransparent(0)
		{
		}

		// Opaque RGB565 image.
		constexpr Image(const uint16_t* pPixels, int32_t widthPix, int32_t heightPix)
			: pData(pPixels), width(widthPix), height(heightPix), format(PixelFormat::RGB565),
			  bHasTransparency(false), colTransparent(0)
		{
		}

		// RGB565 image with one colour (given in ARGB) acting as transparent.
		constexpr Image(const uint16_t* pPixels, int32_t widthPix, int32_t heightPix, uint32_t colTransparentARGB)
			: pData(pPixels), width(widthPix), height(heightPix), format(PixelFormat::RGB565),
			  bHasTransparency(true), colTransparent(colTransparentARGB)
		{
		}
	};

	// Calculates where content of size contentSizePix is to be placed within the
	// area (areaTopLeftPix, areaSizePix), given an alignment.
	// Content larger than the area yields a top-left outside the area; the
	// DisplayAdapter is expected to clip to the area.
	//
	// This lives in CleanGUI rather than in a DisplayAdapter on purpose: every
	// adapter must align identically, so the arithmetic is shared, not copied.
	inline Vec2 calcAlignedTopLeftPix(const Vec2& areaTopLeftPix, const Vec2& areaSizePix,
	                                  const Vec2& contentSizePix, Alignment alignment)
	{
		Vec2 topLeftPix(areaTopLeftPix.x, areaTopLeftPix.y);

		switch (alignment)
		{
		case Alignment::TopMid:
		case Alignment::MidMid:
		case Alignment::BottomMid:
			topLeftPix.x += (areaSizePix.x - contentSizePix.x) / 2;
			break;

		case Alignment::TopRight:
		case Alignment::MidRight:
		case Alignment::BottomRight:
			topLeftPix.x += (areaSizePix.x - contentSizePix.x);
			break;

		default:	// TopLeft, MidLeft, BottomLeft: x stays at the left edge.
			break;
		}

		switch (alignment)
		{
		case Alignment::MidLeft:
		case Alignment::MidMid:
		case Alignment::MidRight:
			topLeftPix.y += (areaSizePix.y - contentSizePix.y) / 2;
			break;

		case Alignment::BottomLeft:
		case Alignment::BottomMid:
		case Alignment::BottomRight:
			topLeftPix.y += (areaSizePix.y - contentSizePix.y);
			break;

		default:	// TopLeft, TopMid, TopRight: y stays at the top edge.
			break;
		}

		return topLeftPix;
	}
};
