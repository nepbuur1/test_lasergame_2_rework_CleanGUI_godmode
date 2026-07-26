// by Marius Versteegen, 2024

#pragma once
#include <crt_ITouchListener.h>
#include <crt_CleanRTOS.h>
#include "crt_Image.h"

namespace crt
{
	// The interface below is part of the IDisplay interface.
	// Both the DisplayAdapter and AsyncDisplay (when including CleanGUI_async library) 
	// implement these functions in a synchronous manner.
	class IDisplaySynchrone
	{
	public:
		// Moet bekend zijn voordat iemand getScreenSize() aanroept, anders is het
		// antwoord de portretmaat van het paneel in plaats van wat er straks op
		// het scherm staat.
		virtual void	setRotation(uint8_t rotation) = 0;
		virtual Vec2	getScreenSize() = 0;
		virtual int16_t getTextWidth(const char* str, uint8_t font) = 0;
		virtual int16_t getFontHeight(uint8_t font) = 0;
		virtual Vec2    getPrintCursor() = 0;
	};

	// The interface below is part of the IDisplay interface.
	// DisplayAdapters typically implement its functions in a synchronous manner.
	// AsyncDisplay implements these functions in an asynchronous manner though.
	class IDisplayDraw
	{
	public:
		// Restricts every subsequent drawing command to this rectangle, until
		// clearClipRect(). Used to repaint just a part of the screen - for
		// instance the strip a widget vacated when it moved - without every
		// widget that happens to overlap it painting outside that strip.
		// Only one clip is active at a time; setting a new one replaces it.
		// Note that these go through the command queue like any other drawing
		// command, so the clip applies in queue order rather than at call time.
		virtual void setClipRect(const Vec2& topLeftPix, const Vec2& sizePix) = 0;
		virtual void clearClipRect() = 0;

		virtual void fillScreen(uint32_t color) = 0;
		virtual void drawRect(const Vec2& topLeftPix, const Vec2& sizePix, uint32_t color) = 0;
		virtual void fillRect(const Vec2& topLeftPix, const Vec2& sizePix, uint32_t color) = 0;
		virtual void drawRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t color) = 0;
		virtual void fillRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t color) = 0;
		virtual void fillSmoothRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t colFg, uint32_t colBg) = 0;

		// A filled rectangle that looks raised, like a physical key: a one pixel
		// colShadow border all around, and a colHighlight line just inside it
		// along the top and the left. So from the outside inwards on the lit
		// sides you get shadow, then gloss, then the face - the shadow being the
		// gap next to the key - while the bottom and right only get shadow.
		// BevelStyle::Sunken puts the gloss along the bottom and right instead,
		// which reads as recessed rather than raised.
		// One call rather than half a dozen fillRects, which matters when a whole
		// keyboard is redrawn through a command queue.
		virtual void fillBeveledRect(const Vec2& topLeftPix, const Vec2& sizePix, uint32_t colFg, uint32_t colHighlight, uint32_t colShadow, BevelStyle style) = 0;
		virtual void drawString(const char* str, const Vec2& bottomLeftPix, Alignment alignment, uint8_t font, uint8_t fontScale, uint32_t color) = 0;

		// Same as drawString, but the glyph cells are painted with colBg instead of
		// being left transparent, and whatever the text does not cover within
		// paddingSizePix is blanked with colBg as well.
		//
		// This is the flicker-free way to replace a label: the new text is written
		// and the remains of the previous, longer text are blanked in that same
		// single pass. There is no moment in between where the area is empty, so
		// no background needs to be painted first - which is exactly what makes an
		// update visibly flicker.
		//
		// paddingSizePix is the area reserved for the text, positioned around
		// bottomLeftPix by the same alignment rule (see calcAlignedTopLeftPix).
		// Nothing is drawn outside it, so pass the bounds of the widget the text
		// lives in. A zero size means "no padding": only the glyph cells are
		// painted, and a previous longer text is NOT cleaned up.
		virtual void drawStringOpaque(const char* str, const Vec2& bottomLeftPix, Alignment alignment, uint8_t font, uint8_t fontScale, uint32_t colFg, uint32_t colBg, const Vec2& paddingSizePix) = 0;

		// Draws an image inside the rectangle (topLeftPix, sizePix).
		// The image is placed within that rectangle according to alignment (see
		// calcAlignedTopLeftPix) and clipped to it: an image larger than the
		// rectangle is cropped, a smaller one leaves the surrounding pixels
		// untouched.
		// The image data is not copied - see the lifetime note in crt_Image.h.
		virtual void drawImage(const Vec2& topLeftPix, const Vec2& sizePix, const Image& image, Alignment alignment) = 0;
	};

	// The interface below is part of the IDisplay interface.
	// These functions define a "print context" within AsyncDisplay 
	// which is maintained for each task separately.
	// The print functions are executed in the context of that "print context".
	// The (synchronous) implementation of a DisplayAdapter of the functions below typically
	// ignores the pTask parameter (a nullptr can be passed then).
	// When using CleanRTOS, pass Task pointers instead to allow asynchronous 
	// multi - threaded access to the display.
	class IDisplayPrint
	{
	public:
		virtual void setPrintColor(void* pTask, uint32_t color) = 0;
		virtual void setPrintFont(void* pTask, uint8_t font) = 0;
		virtual void setPrintScale(void* pTask, uint8_t fontScale) = 0;
		virtual void setPrintCursor(void* pTask, const Vec2& cursorPos) = 0;
		virtual void setPrintWrap(void* pTask, bool bWrapX) = 0;
		virtual void print(void* pTask, const char* str) = 0;
		virtual void println(void* pTask, const char* str) = 0;
	};

	// The interface below is used for use with touch displays.
	// If your display does not support touch, you can just override the functions below
	// with empty stubs.
	class IDisplayTouch
	{
	public:
		virtual void pollTouch(void* pCallingTask) = 0;
		virtual void addTouchListener(ITouchListener* pTouchListener) = 0;
		virtual void touchCalibrate(uint8_t rotation, uint8_t font) = 0;
	};

	class IDisplay : public IDisplaySynchrone, public IDisplayDraw, public IDisplayPrint, public IDisplayTouch
	{
	};
};
