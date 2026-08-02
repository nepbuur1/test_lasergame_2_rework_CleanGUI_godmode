// by Marius Versteegen, 2024

#pragma once
#include <assert.h>
#include <crt_FreeRTOS.h>
#include "crt_typesAndConstants.h"
#include "crt_Image.h"
#include "crt_IDisplay.h"
#include "crt_Widget.h"

namespace crt
{
	template <size_t MaxNofChildren> 
	class Panel : public Widget<MaxNofChildren>
	{
	private:
		uint32_t _colFg, _colBg;
		int32_t _cornerRadius;
		int32_t _cornerRadiusPix;

		int32_t _cornerRadiusWhenCornerRadiusPixWasCalculated;

		// Raised-key look: a colShadow border with a colHighlight gloss just
		// inside it along the top and left. Off unless setBevel() was called.
		// A bevel and a corner radius do not combine - the bevel wins, because
		// its lines are straight.
		bool _bBeveled;
		uint32_t _colHighlight, _colShadow;
		BevelStyle _bevelStyle;

		// nullptr: the panel draws itself as a plain (rounded) rectangle.
		// Otherwise it blits this image instead. The Image is NOT copied - the
		// caller guarantees its lifetime (trivially so for const data in flash).
		const Image* _pImage;

		// Which frame of a spritesheet is on show; -1 for "the whole image".
		// This is per panel and not part of the Image on purpose: an Image is
		// shared, constexpr and lives in flash, so it cannot hold a selection
		// that changes - and two panels showing the same sheet want their own.
		int32_t _frameRow, _frameCol;
	public:
		using WBase = Widget<MaxNofChildren>;

		Panel(const char* strName, const Vec2& locPos, CoordType coordTypeLocPos,
			  const Vec2& size, int32_t cornerRadius, CoordType coordTypeSize,
			  Alignment alignment, uint32_t colFg, uint32_t colBg, bool bDrawable = true) :
			  WBase(strName, locPos, coordTypeLocPos, size, coordTypeSize, alignment, bDrawable),
			  _colFg(colFg), _colBg(colBg), _cornerRadius(cornerRadius),
			  _cornerRadiusPix(-1),
			  _cornerRadiusWhenCornerRadiusPixWasCalculated(-1),
			  _bBeveled(false), _colHighlight(0), _colShadow(0), _bevelStyle(BevelStyle::Raised),
			  _pImage(nullptr), _frameRow(-1), _frameCol(-1)
		{
		}

		// Same, but the panel shows an image. The image is aligned within the
		// panel using the alignment parameter, and clipped to the panel.
		Panel(const char* strName, const Vec2& locPos, CoordType coordTypeLocPos,
			  const Vec2& size, int32_t cornerRadius, CoordType coordTypeSize,
			  Alignment alignment, uint32_t colFg, uint32_t colBg,
			  const Image& image, bool bDrawable = true) :
			  WBase(strName, locPos, coordTypeLocPos, size, coordTypeSize, alignment, bDrawable),
			  _colFg(colFg), _colBg(colBg), _cornerRadius(cornerRadius),
			  _cornerRadiusPix(-1),
			  _cornerRadiusWhenCornerRadiusPixWasCalculated(-1),
			  _bBeveled(false), _colHighlight(0), _colShadow(0), _bevelStyle(BevelStyle::Raised),
			  _pImage(&image), _frameRow(-1), _frameCol(-1)
		{
		}

		Panel() : Panel("", Vec2(0,0), CoordType::Pixels, Vec2(0,0), 0, CoordType::Pixels, Alignment::TopLeft, 0, 0)
		{
		}

		// convenience function to set most properties at once.
		void setPanelProps(const char* strName, const Vec2& locPos, CoordType coordTypeLocPos,
			  const Vec2& size, int32_t cornerRadius, CoordType coordTypeSize, 
			  Alignment alignment, uint32_t colFg, uint32_t colBg)
		{
			WBase::setWidgetProps(strName, locPos, coordTypeLocPos, size, coordTypeSize, alignment);
			_colFg = colFg;
			_colBg = colBg;
			_cornerRadius = cornerRadius;
		}

		// Makes this panel look like a raised key. See IDisplayDraw::fillBeveledRect
		// for what gets drawn where.
		void setBevel(uint32_t colHighlight, uint32_t colShadow, BevelStyle style = BevelStyle::Raised)
		{
			_bBeveled = true;
			_colHighlight = colHighlight;
			_colShadow = colShadow;
			_bevelStyle = style;
		}

		// How far content must stay away from the edges so it does not paint over
		// the bevel. Two pixels: the shadow border plus the gloss line.
		int32_t getBevelInsetPix()
		{
			return _bBeveled ? 2 : 0;
		}

		void clearBevel()
		{
			_bBeveled = false;
		}

		uint32_t getColFg()
		{
			return _colFg;
		}

		uint32_t getColBg()
		{
			return _colBg;
		}

		// Pass nullptr to turn the panel back into a plain coloured rectangle.
		// Any frame selection is dropped: it belonged to the previous image, and
		// silently keeping it would show a different picture than asked for.
		void setImage(const Image* pImage)
		{
			_pImage = pImage;
			_frameRow = -1;
			_frameCol = -1;
		}

		const Image* getImage()
		{
			return _pImage;
		}

		// Shows one frame of a spritesheet instead of the whole sheet. Rows and
		// columns are those of Image::nofRows / Image::nofCols, counted from 0;
		// out of range wraps around, so stepping through frames needs no bounds
		// checking at the call site.
		void setImageFrame(int32_t row, int32_t col)
		{
			if ((_pImage == nullptr) || (_pImage->nofRows < 1) || (_pImage->nofCols < 1))
			{
				return;
			}
			_frameRow = ((row % _pImage->nofRows) + _pImage->nofRows) % _pImage->nofRows;
			_frameCol = ((col % _pImage->nofCols) + _pImage->nofCols) % _pImage->nofCols;
		}

		// Back to showing the image as a whole.
		void clearImageFrame()
		{
			_frameRow = -1;
			_frameCol = -1;
		}

		bool hasImageFrame()
		{
			return (_frameRow >= 0) && (_frameCol >= 0);
		}

		int32_t getImageFrameRow()
		{
			return _frameRow;
		}

		int32_t getImageFrameCol()
		{
			return _frameCol;
		}

		/* override */ WidgetType getWidgetType()
		{
			return WidgetType::Panel;
		}

		// See Widget::coversOwnRect(). A panel fills its whole rect unless it has
		// rounded corners (which leave the corners alone) or shows an image that
		// is allowed to let something through.
		/* override */ bool coversOwnRect()
		{
			// _cornerRadius rather than _cornerRadiusPix: that one is only
			// computed at draw time and is -1 until then. A bevel overrules the
			// radius - see showPanel - and does fill everything.
			if ((_cornerRadius > 0) && !_bBeveled)
			{
				return false;
			}

			// needsBackground() true means colFg goes over the full rect. False
			// means only the image paints, so it has to be an opaque one - and an
			// opaque image that left part of the panel uncovered would have made
			// needsBackground() true in the first place.
			if (needsBackground(WBase::getSizePix()))
			{
				return true;
			}
			return (_pImage != nullptr) && !_pImage->bHasTransparency;
		}

		// show immediately draws it. hide immediately undraws it.
		// note: _widgetProps only serves as a node in the display-tree.
		//       It conains the props and links to children.
		//       The actual (un-)display functionality is to be implemented
		//       by the specific widget class (like TouchScreenButton),
		//       which is derived from IWidget as well.
		/*override*/ void paint()
		{
			showPanel();
		}

		/*override*/ void show(bool bIncludeChildren)
		{
			showPanel();
			Widget<MaxNofChildren>::show(bIncludeChildren); // set the boolean bVisible to remember.
		}

		// show immediately draws it. hide immediately undraws it.
		//
		// Wissen doet hij alleen als er ook echt iets van hem stond. Een pagina
		// die na het opbouwen meteen verborgen wordt - wat hier elk van de drie
		// pagina's doet - heeft nog nooit een pixel gezet, en vulde anders bij
		// dat verbergen alsnog zijn eigen vlak met colBg. Dat hoeft niet alleen
		// niet, het is ook fout zodra een andere taak daar ondertussen wel iets
		// heeft neergezet: bij het opstarten viel op die manier het wisvlak van
		// de peddel (135,167)-(185,179) precies over de startknop.
		/*override*/ void hide(bool bIncludeChildren)
		{
			if (WBase::isShown())
			{
				hidePanel();
			}
			WBase::hide(bIncludeChildren); // sets the boolean bVisible to false to remember.
		}

	private:
		void updateCornerRadiusPixIfNeeded()
		{
			if (_cornerRadius != _cornerRadiusWhenCornerRadiusPixWasCalculated)
			{
				// apply same scaling as for size to sizePix:
				if (WBase::getSize().x != 0)
				{
					int64_t tempRadiusPix = (int64_t)_cornerRadius * (int64_t)WBase::getSizePix().x / (int64_t)WBase::getSize().x;
					_cornerRadiusPix = int32_t(tempRadiusPix);
				}
				else
				{
					ESP_LOGI("Zero size of panel?", "div by zero avoided");
				}
			}
		}

		// An image is drawn on top of the panel background. Painting that
		// background first is wasted bus traffic when the image covers it
		// anyway - and it is plain wrong for a transparent image, because there
		// it is exactly the pixels underneath (drawn by the parent) that should
		// stay visible.
		// Note: an opaque image that covers the whole panel therefore also
		// suppresses the rounded corners - a rectangular blit cannot honour
		// them. Give such a panel a transparent image if you want rounded
		// corners, or make the panel bigger than the image.
		bool needsBackground(const Vec2& sizePix)
		{
			if (_pImage == nullptr)
			{
				return true;
			}
			if (_pImage->bHasTransparency)
			{
				return false;
			}
			// Opaque image: only paint the background if the image leaves part
			// of the panel uncovered. With a frame selected it is that frame
			// which does the covering, so its size is what counts.
			int32_t widthPix  = _pImage->width;
			int32_t heightPix = _pImage->height;
			if (hasImageFrame())
			{
				widthPix  /= _pImage->nofCols;
				heightPix /= _pImage->nofRows;
			}
			return (widthPix < sizePix.x) || (heightPix < sizePix.y);
		}

		void showPanel()
		{
			if (!WBase::isDrawable())
			{
				return;
			}

			updateCornerRadiusPixIfNeeded();

			Vec2 pos = WBase::getGlobPosOfParent() + WBase::getLocPosPix();
			Vec2 size = WBase::getSizePix();
			//ESP_LOGI("Panel", "showPanel called: pos(%ld,%ld) size(%ld,%ld) colFg=0x%08lX cornerRadius=%ld",
			//	(long)pos.x, (long)pos.y, (long)size.x, (long)size.y, (unsigned long)_colFg, (long)_cornerRadiusPix);

			if (needsBackground(size))
			{
				if (_bBeveled)
				{
					WBase::getDisplay()->fillBeveledRect(pos, size, _colFg, _colHighlight, _colShadow, _bevelStyle);
				}
				else if (_cornerRadiusPix > 0)
				{
					WBase::getDisplay()->fillSmoothRoundRect(pos, size, _cornerRadiusPix, _colFg, _colBg);
				}
				else
				{
					WBase::getDisplay()->fillRect(pos, size, _colFg);
				}
			}

			if (_pImage != nullptr)
			{
				WBase::getDisplay()->drawImage(pos, size, *_pImage, WBase::getAlignment(), _frameRow, _frameCol);
			}
			//ESP_LOGI("Panel shown", "globparent Y = %d", _widgetProps.getGlobPosOfParent().y);
			//ESP_LOGI("Panel shown", "locpospix Y = %d", _widgetProps.getLocPosPix().y);
			//ESP_LOGI("Panel shown", "locpos Y = %d", _widgetProps.getLocPos().y);
		}

		void hidePanel()
		{
			if (!WBase::isDrawable())
			{
				return;
			}

			WBase::getDisplay()->fillRect(WBase::getGlobPosOfParent() + WBase::getLocPosPix(), WBase::getSizePix(), _colBg);
		}
	};
};
