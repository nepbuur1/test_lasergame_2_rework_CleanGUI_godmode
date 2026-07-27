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
		RGB565 = 0,		// 16 bpp, one uint16_t per pixel, in host byte order.

		// 8 bpp: one byte per pixel, and that byte is an index into pPalette.
		// Half the flash of RGB565, and - as long as the image and the target
		// share a palette - blitting degenerates to copying bytes: no conversion
		// per pixel at all. That, rather than the colour depth, is what makes
		// paletted graphics fast.
		// Index 0 is the transparent one, by agreement with the converter.
		PAL8 = 1
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
	//
	// Spritesheets: nofRows and nofCols say into how many equally sized frames
	// the image is divided; 1 x 1 (the default) means "just one picture".
	// Which frame is on show is NOT stored here: an Image describes the bitmap,
	// not an appearance on screen - it has no position either, and for the same
	// reason. It is shared, constexpr and lives in flash, so it could not hold a
	// changing selection anyway. Panel::setImageFrame() is where that lives.
	class Image
	{
	public:
		const void* pData;
		int32_t     width;
		int32_t     height;
		PixelFormat format;
		bool        bHasTransparency;
		uint32_t    colTransparent;		// ARGB, or paletteIndex(...) for PAL8
		int32_t     nofRows;			// spritesheet rows,    1 if not a sheet
		int32_t     nofCols;			// spritesheet columns, 1 if not a sheet

		// PAL8 only: the 256 colours the indices refer to, as RGB565. Lives in
		// flash next to the pixels; nullptr for RGB565 images.
		const uint16_t* pPalette;

	public:
		constexpr Image()
			: pData(nullptr), width(0), height(0), format(PixelFormat::RGB565),
			  bHasTransparency(false), colTransparent(0), nofRows(1), nofCols(1), pPalette(nullptr)
		{
		}

		// Opaque RGB565 image.
		constexpr Image(const uint16_t* pPixels, int32_t widthPix, int32_t heightPix)
			: pData(pPixels), width(widthPix), height(heightPix), format(PixelFormat::RGB565),
			  bHasTransparency(false), colTransparent(0), nofRows(1), nofCols(1), pPalette(nullptr)
		{
		}

		// RGB565 image with one colour (given in ARGB) acting as transparent.
		constexpr Image(const uint16_t* pPixels, int32_t widthPix, int32_t heightPix, uint32_t colTransparentARGB)
			: pData(pPixels), width(widthPix), height(heightPix), format(PixelFormat::RGB565),
			  bHasTransparency(true), colTransparent(colTransparentARGB), nofRows(1), nofCols(1), pPalette(nullptr)
		{
		}

		// Opaque RGB565 spritesheet.
		// Note that the parameter count is what distinguishes this constructor
		// from the transparent one above: neither has default arguments, on
		// purpose. With defaults, Image(p, w, h, 0x00FF00FF) would silently
		// match nofRowsSheet instead of colTransparentARGB, since a hex literal
		// that fits in an int is an int.
		constexpr Image(const uint16_t* pPixels, int32_t widthPix, int32_t heightPix,
		                int32_t nofRowsSheet, int32_t nofColsSheet)
			: pData(pPixels), width(widthPix), height(heightPix), format(PixelFormat::RGB565),
			  bHasTransparency(false), colTransparent(0), nofRows(nofRowsSheet), nofCols(nofColsSheet), pPalette(nullptr)
		{
		}

		// RGB565 spritesheet with a transparent colour.
		constexpr Image(const uint16_t* pPixels, int32_t widthPix, int32_t heightPix, uint32_t colTransparentARGB,
		                int32_t nofRowsSheet, int32_t nofColsSheet)
			: pData(pPixels), width(widthPix), height(heightPix), format(PixelFormat::RGB565),
			  bHasTransparency(true), colTransparent(colTransparentARGB), nofRows(nofRowsSheet), nofCols(nofColsSheet), pPalette(nullptr)
		{
		}

		// --- PAL8 ----------------------------------------------------------
		// The first parameter is a uint8_t*, so these can never be confused with
		// the RGB565 constructors above.
		// bTransparent says whether index 0 means "leave what is underneath";
		// which index that is, is fixed by agreement with the converter, so it
		// needs no parameter of its own.

		constexpr Image(const uint8_t* pIndices, int32_t widthPix, int32_t heightPix,
		                const uint16_t* pPaletteRGB565)
			: pData(pIndices), width(widthPix), height(heightPix), format(PixelFormat::PAL8),
			  bHasTransparency(false), colTransparent(0), nofRows(1), nofCols(1), pPalette(pPaletteRGB565)
		{
		}

		constexpr Image(const uint8_t* pIndices, int32_t widthPix, int32_t heightPix,
		                const uint16_t* pPaletteRGB565, bool bTransparent)
			: pData(pIndices), width(widthPix), height(heightPix), format(PixelFormat::PAL8),
			  bHasTransparency(bTransparent), colTransparent(paletteIndex(0)), nofRows(1), nofCols(1),
			  pPalette(pPaletteRGB565)
		{
		}

		constexpr Image(const uint8_t* pIndices, int32_t widthPix, int32_t heightPix,
		                const uint16_t* pPaletteRGB565, bool bTransparent,
		                int32_t nofRowsSheet, int32_t nofColsSheet)
			: pData(pIndices), width(widthPix), height(heightPix), format(PixelFormat::PAL8),
			  bHasTransparency(bTransparent), colTransparent(paletteIndex(0)),
			  nofRows(nofRowsSheet), nofCols(nofColsSheet), pPalette(pPaletteRGB565)
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
